#!/usr/bin/env python3
"""Differential test of asterix-dump against an independent ASTERIX implementation.

Uses libasterix (https://pypi.org/project/libasterix/, generated from the
machine-readable EUROCONTROL specifications at
https://github.com/zoranbosnjak/asterix-specs) as the reference:

  * random mode: builds random but structurally valid records for CAT034
    (ed. 1.29), CAT048 (ed. 1.32) and CAT063 (ed. 1.7) by walking the
    reference specification, decodes them with both implementations and
    compares every field;
  * capture mode: decodes the UDP payloads of pcap files with both
    implementations and compares every field.

Usage:
    pip install libasterix
    scripts/crosscheck.py --dump build/asterix-dump                 # random records
    scripts/crosscheck.py --dump build/asterix-dump capture.pcap    # recorded traffic

Exits with status 1 on any mismatch.
"""

import argparse
import json
import math
import os
import random
import struct
import subprocess
import sys
import tempfile

from asterix.base import (
    Bits, Compound, ContentInteger, ContentQuantity, ContentString, Element, Explicit, Extended, Group, Item,
    RawDatablock, Repetitive, RuleVariationContextFree, Spare, StringICAO, UapItem,
)
import asterix.generated as gen

SPECS = {34: gen.Cat_034_1_29, 48: gen.Cat_048_1_32, 63: gen.Cat_063_1_7}


ICAO_CODES = [0x20] + list(range(0x01, 0x1B)) + list(range(0x30, 0x3A))


# ---------------------------------------------------------------- generation

class BitWriter:
    def __init__(self):
        self.bits = []

    def put(self, value, width):
        for i in reversed(range(width)):
            self.bits.append((value >> i) & 1)

    def to_bytes(self):
        assert len(self.bits) % 8 == 0, "items must be byte aligned"
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            byte = 0
            for b in self.bits[i:i + 8]:
                byte = (byte << 1) | b
            out.append(byte)
        return bytes(out)


def variation_of(nonspare):
    rule = nonspare.cv_rule
    if issubclass(rule, RuleVariationContextFree):
        return rule.cv_variation
    return rule.cv_default_variation


def content_of(element):
    rule = element.cv_rule
    return rule.cv_content if hasattr(rule, "cv_content") else rule.cv_default_content


def gen_variation(rng, var, w):
    if issubclass(var, Element):
        content = content_of(var)
        if issubclass(content, ContentString) and content.cv_string_type is StringICAO:
            for _ in range(var.cv_bit_size // 6):
                w.put(rng.choice(ICAO_CODES), 6)
        else:
            w.put(rng.getrandbits(var.cv_bit_size), var.cv_bit_size)
    elif issubclass(var, Group):
        for item, size in var.cv_items_list:
            if issubclass(item, Spare):
                w.put(rng.getrandbits(size) if rng.random() < 0.2 else 0, size)  # spare bits must be ignored
            else:
                gen_variation(rng, variation_of(item.cv_non_spare), w)
    elif issubclass(var, Extended):
        groups = rng.randint(1, len(var.cv_items_list))
        for index, group in enumerate(var.cv_items_list[:groups]):
            for entry in group:
                if entry is None:
                    w.put(1 if index + 1 < groups else 0, 1)
                elif issubclass(entry[0], Spare):
                    w.put(0, entry[1])
                else:
                    gen_variation(rng, variation_of(entry[0].cv_non_spare), w)
    elif issubclass(var, Repetitive):
        if var.cv_rep_bytes is None:  # FX-terminated repetition
            count = rng.randint(1, 4)
            for i in range(count):
                gen_variation(rng, var.cv_variation, w)
                w.put(1 if i + 1 < count else 0, 1)
        else:
            count = rng.randint(0, 3)
            w.put(count, 8 * var.cv_rep_bytes)
            for _ in range(count):
                gen_variation(rng, var.cv_variation, w)
    elif issubclass(var, Explicit):
        length = rng.randint(1, 6)
        w.put(length, 8)
        for _ in range(length - 1):
            w.put(rng.getrandbits(8), 8)
    elif issubclass(var, Compound):
        chosen = [i for i, sub in enumerate(var.cv_items_list) if sub is not None and rng.random() < 0.6]
        write_fspec(w, chosen)
        for i in chosen:
            gen_variation(rng, variation_of(var.cv_items_list[i]), w)
    else:
        raise TypeError(var)


def write_fspec(w, indexes):
    octets = max(1, (max(indexes) // 7 + 1) if indexes else 1)
    for octet in range(octets):
        for bit in range(7):
            w.put(1 if octet * 7 + bit in indexes else 0, 1)
        w.put(1 if octet + 1 < octets else 0, 1)


def gen_record(rng, spec):
    record = spec.cv_record
    chosen = [i for i, item in enumerate(record.cv_items_list)
              if issubclass(item, UapItem) and rng.random() < 0.5]
    if not chosen:
        chosen = [0]
    w = BitWriter()
    write_fspec(w, chosen)
    for i in chosen:
        gen_variation(rng, variation_of(record.cv_items_list[i].cv_non_spare), w)
    return w.to_bytes()


def gen_block(rng, category, records):
    body = b"".join(gen_record(rng, SPECS[category]) for _ in range(records))
    return bytes([category]) + struct.pack(">H", len(body) + 3) + body


# ---------------------------------------------------------------- reference

def ref_nonspare(nsp):
    rule = nsp.arg
    if isinstance(rule, RuleVariationContextFree):
        return ref_variation(rule.arg)
    var, _ = type(rule).cv_default_variation._parse(rule.bs)
    return ref_variation(var)


def ref_variation(v):
    if isinstance(v, Element):
        cls = type(v)
        content = content_of(cls)
        raw = v.bs.to_uinteger()
        if issubclass(content, ContentQuantity):
            return content.cv_signedness.convert(cls.cv_bit_size, raw) * content.cv_lsb
        if issubclass(content, ContentString):
            return content.cv_string_type.to_string(cls.cv_bit_size, raw)
        if issubclass(content, ContentInteger):
            return content.cv_signedness.convert(cls.cv_bit_size, raw)
        return raw
    if isinstance(v, Group):
        return {i.arg.cv_name: ref_nonspare(i.arg) for i in v.arg if isinstance(i, Item)}
    if isinstance(v, Extended):
        return {i.arg.cv_name: ref_nonspare(i.arg) for group in v.arg for i in group if isinstance(i, Item)}
    if isinstance(v, Repetitive):
        return [ref_variation(x) for x in v.arg]
    if isinstance(v, Compound):
        return {name: ref_nonspare(nsp) for name, nsp in v.arg.items()}
    if isinstance(v, Explicit):
        return v.get_bytes().hex().upper()
    raise TypeError(v)


def reference_blocks(datagram):
    """[(category, [record dict, ...] or None if not decodable)]"""
    blocks = RawDatablock.parse(Bits.from_bytes(datagram))
    if isinstance(blocks, ValueError):
        raise ValueError(f"reference framing error: {blocks}")
    out = []
    for db in blocks:
        category = db.get_category()
        spec = SPECS.get(category)
        if spec is None:
            out.append((category, None))
            continue
        records = spec.cv_uap.parse(db.get_raw_records())
        if isinstance(records, ValueError):
            raise ValueError(f"reference rejects CAT{category:03d} block: {records}")
        converted = []
        for rec in records:
            d = {}
            for name, nsp in rec.items_regular.items():
                key = name if name in ("SP", "RE") else "I" + name
                d[key] = ref_nonspare(nsp)
            converted.append(d)
        out.append((category, converted))
    return out


# ---------------------------------------------------------------- comparison

def same_number(a, b):
    return math.isclose(float(a), float(b), rel_tol=1e-12, abs_tol=1e-12)


def compare(path, mine, ref, errors):
    if isinstance(ref, dict):
        if not isinstance(mine, dict):
            errors.append(f"{path}: expected object, got {mine!r}")
            return
        for key in sorted(set(ref) | set(mine)):
            if key not in mine:
                errors.append(f"{path}.{key}: missing (reference {ref[key]!r})")
            elif key not in ref:
                errors.append(f"{path}.{key}: unexpected {mine[key]!r}")
            else:
                compare(f"{path}.{key}", mine[key], ref[key], errors)
    elif isinstance(ref, list):
        if not isinstance(mine, list) or len(mine) != len(ref):
            errors.append(f"{path}: expected {len(ref)} entries, got {mine!r}")
            return
        for i, (a, b) in enumerate(zip(mine, ref)):
            compare(f"{path}[{i}]", a, b, errors)
    elif isinstance(ref, str):
        if path.endswith(".I240"):
            mine, ref = mine.rstrip(), ref.rstrip()
        if mine != ref:
            errors.append(f"{path}: {mine!r} != reference {ref!r}")
    elif isinstance(ref, (int, float)) and not isinstance(ref, bool):
        if isinstance(mine, bool) or not isinstance(mine, (int, float)) or not same_number(mine, ref):
            errors.append(f"{path}: {mine!r} != reference {ref!r}")
    else:
        errors.append(f"{path}: unexpected reference value {ref!r}")


def normalize(category, record):
    """asterix-dump prints some raw fields as hexadecimal strings; the reference uses integers."""
    record = dict(record)
    if category == 48:
        if "I240" in record:
            # Characters outside the ICAO set: the reference drops them, asterix-dump
            # shows code 0 as a space and other unused codes as '?'.
            record["I240"] = record["I240"].replace("?", "")
        for key in ("I220", "I260"):
            if key in record:
                record[key] = int(record[key], 16)
        if "I250" in record:
            record["I250"] = [dict(e, MBDATA=int(e["MBDATA"], 16)) for e in record["I250"]]
    return record


def run_dump(dump, path, fmt):
    proc = subprocess.run([dump, "--json", "--format", fmt, path], capture_output=True, text=True)
    lines = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    return proc.returncode, lines


def check_datagrams(label, datagrams, dump_lines, errors):
    """datagrams: [bytes]; dump_lines: JSON objects in output order."""
    blocks = [line for line in dump_lines if "error" not in line]
    for line in dump_lines:
        if "error" in line:
            errors.append(f"{label}: asterix-dump reported {line['error']}")
    index = 0
    records = 0
    for number, datagram in enumerate(datagrams):
        for block_number, (category, ref_records) in enumerate(reference_blocks(datagram)):
            if index >= len(blocks):
                errors.append(f"{label}: asterix-dump output ends early")
                return records
            mine = blocks[index]
            index += 1
            where = f"{label} datagram {number} block {block_number} CAT{category:03d}"
            if mine["category"] != category:
                errors.append(f"{where}: category {mine['category']}")
                continue
            if ref_records is None:
                continue
            if len(mine["records"]) != len(ref_records):
                errors.append(f"{where}: {len(mine['records'])} records, reference {len(ref_records)}")
                continue
            for i, (a, b) in enumerate(zip(mine["records"], ref_records)):
                compare(f"{where} record {i}", normalize(category, a), b, errors)
                records += 1
    if index != len(blocks):
        errors.append(f"{label}: asterix-dump produced {len(blocks) - index} extra blocks")
    return records


def pcap_udp_payloads(path):
    data = open(path, "rb").read()
    magic = struct.unpack("<I", data[:4])[0]
    endian = {0xA1B2C3D4: "<", 0xA1B23C4D: "<", 0xD4C3B2A1: ">", 0x4D3CB2A1: ">"}.get(magic)
    if endian is None:
        raise SystemExit(f"{path}: only classic pcap files are supported here")
    link = struct.unpack(endian + "I", data[20:24])[0] & 0xFFFF
    if link != 1:
        raise SystemExit(f"{path}: only Ethernet captures are supported here")
    pos, out = 24, []
    while pos + 16 <= len(data):
        incl = struct.unpack(endian + "I", data[pos + 8:pos + 12])[0]
        frame = data[pos + 16:pos + 16 + incl]
        pos += 16 + incl
        offset, ethertype = 14, struct.unpack(">H", frame[12:14])[0]
        while ethertype in (0x8100, 0x88A8):
            ethertype = struct.unpack(">H", frame[offset + 2:offset + 4])[0]
            offset += 4
        if ethertype != 0x0800 or frame[offset + 9] != 17:
            continue
        if struct.unpack(">H", frame[offset + 6:offset + 8])[0] & 0x3FFF:
            continue  # fragment
        ihl = (frame[offset] & 0x0F) * 4
        udp = frame[offset + ihl:]
        length = struct.unpack(">H", udp[4:6])[0]
        out.append(bytes(udp[8:length]))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dump", required=True, help="path to the asterix-dump executable")
    parser.add_argument("--records", type=int, default=20000, help="random records per category")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("captures", nargs="*", help="pcap files to compare instead of random records")
    args = parser.parse_args()

    errors = []
    total = 0
    if args.captures:
        for path in args.captures:
            _, lines = run_dump(args.dump, path, "pcap")
            total += check_datagrams(path, pcap_udp_payloads(path), lines, errors)
    else:
        rng = random.Random(args.seed)
        datagrams = []
        remaining = {c: args.records for c in SPECS}
        while any(remaining.values()):
            datagram = b""
            for category in SPECS:
                if remaining[category]:
                    n = min(remaining[category], rng.randint(1, 5))
                    remaining[category] -= n
                    datagram += gen_block(rng, category, n)
            datagrams.append(datagram)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "random.hex")
            with open(path, "w") as f:
                for d in datagrams:
                    f.write(d.hex() + "\n")
            _, lines = run_dump(args.dump, path, "hex")
        total += check_datagrams("random", datagrams, lines, errors)

    for e in errors[:50]:
        print("MISMATCH", e)
    if len(errors) > 50:
        print(f"... and {len(errors) - 50} more")
    print(f"compared {total} records: {'OK' if not errors else f'{len(errors)} mismatches'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
