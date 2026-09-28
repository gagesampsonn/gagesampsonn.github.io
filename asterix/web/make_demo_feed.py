#!/usr/bin/env python3
"""Generates the synthetic ASTERIX feed used by the browser demo.

One Mode S radar (SAC/SIC 16/42) scanning every 4.8 s for 40 scans, with
fictional traffic: en-route jets, a climbing departure, a descending arrival,
an emergency (squawk 7700), a Mode A/C-only VFR flight, a primary-only target,
a track that starts late and one that ends. Each sector of the antenna
rotation becomes one UDP-style datagram holding a CAT048 data block with the
plots of that sector and the CAT034 sector-crossing (or north marker) message.
Every fifth scan an SDPS also reports sensor status in CAT063.

Output: one hex-encoded datagram per line (the format asterix-dump reads).
The encoding is written directly from the EUROCONTROL specifications; when
libasterix is installed (pip install libasterix) every datagram is also
parsed with it and the decoded values are checked against the scenario.

    python3 make_demo_feed.py > demo-feed.hex
"""

import math
import random
import sys

SAC, SIC = 16, 42
SDPS_SIC = 200
START = 14 * 3600 + 5 * 60        # 14:05:00 UTC
PERIOD = 614 / 128                # antenna rotation period, s (exactly representable in I034/041)
SCANS = 40
FT_PER_NM = 6076.12

# callsign, Mode S address, squawk (octal), kind, x0, y0 (NM east/north of the radar),
# track (deg), ground speed (kt), start altitude (ft), vertical rate (ft/min), first scan, last scan
TRAFFIC = [
    ("DLH4TK", 0x3C4B21, 0o2215, "S", -62, 48, 118, 452, 35000, 0, 0, SCANS),
    ("SWR27A", 0x4B1812, 0o1043, "S", 30, 85, 205, 438, 37000, 0, 0, SCANS),
    ("AUA513", 0x440107, 0o4731, "SP", 70, -20, 290, 441, 33000, 0, 0, SCANS),
    ("BAW84KL", 0x4007F3, 0o6120, "S", -85, -30, 65, 467, 39000, 0, 0, SCANS),
    ("EZY19GP", 0x4061A2, 0o7700, "S", 40, 42, 250, 420, 31000, -1800, 0, SCANS),
    ("RYR8QA", 0x4CA6B1, 0o3310, "S", -20, -70, 10, 445, 36000, 0, 0, SCANS),
    ("KLM1795", 0x484C52, 0o5654, "S", 95, 35, 262, 455, 38000, 0, 0, SCANS),
    ("AFR63CV", 0x3944E1, 0o0751, "S", -40, 95, 165, 430, 34000, 0, 0, SCANS),
    ("THY1ZB", 0x4BA9C3, 0o2064, "SP", 12, 18, 40, 310, 9000, 2600, 0, SCANS),
    ("LOT3KR", 0x48AE21, 0o6437, "SP", 55, -60, 330, 330, 21000, -1500, 0, SCANS),
    ("UAE5AF", 0x896143, 0o1276, "S", -95, 60, 100, 480, 40000, 0, 0, SCANS),
    ("HBKFA", 0x4B3F2D, 0o4412, "S", -30, -12, 120, 118, 6500, 0, 0, SCANS),
    (None, None, 0o7000, "AC", -18, 8, 300, 105, 4500, 0, 0, SCANS),
    (None, None, None, "P", 8, -14, 200, 62, None, 0, 0, SCANS),
    ("SAS9DT", 0x4ACA5B, 0o3372, "S", 104, -5, 280, 460, 36000, 0, 0, 24),
    ("EWG7MB", 0x3C6E9F, 0o5520, "S", -110, -80, 45, 440, 30000, 0, 10, SCANS),
]

TYP = {"S": 5, "SP": 7, "AC": 2, "P": 1}          # I048/020 detection type
RAD = {"S": 2, "SP": 0, "AC": 2, "P": 1}          # I048/170 sensor(s) maintaining the track


# ------------------------------------------------------------------ encoding

def u8(v): return bytes([v & 0xFF])
def u16(v): return (v & 0xFFFF).to_bytes(2, "big")
def u24(v): return (v & 0xFFFFFF).to_bytes(3, "big")


def record(items):
    """items: {FRN: bytes}. Returns FSPEC + items in FRN order."""
    last = max(items)
    fspec = bytearray((last + 6) // 7)
    for frn in items:
        fspec[(frn - 1) // 7] |= 0x80 >> ((frn - 1) % 7)
    for i in range(len(fspec) - 1):
        fspec[i] |= 1
    return bytes(fspec) + b"".join(items[k] for k in sorted(items))


def block(category, records):
    body = b"".join(records)
    return bytes([category]) + u16(len(body) + 3) + body


def time_of_day(t): return u24(round(t * 128))


def icao_chars(text):
    v = 0
    for ch in text.ljust(8)[:8]:
        code = ord(ch) - 64 if "A" <= ch <= "Z" else (32 if ch == " " else ord(ch))
        v = (v << 6) | code
    return v.to_bytes(6, "big")


class Mb:
    """56-bit Comm-B message; bits numbered 1..56 from the MSB as in ICAO Doc 9871."""

    def __init__(self):
        self.value = 0

    def put(self, first_bit, width, v):
        self.value |= (v & ((1 << width) - 1)) << (56 - (first_bit + width - 1))
        return self


def bds40(selected_ft):
    """Selected vertical intention: MCP/FCU altitude, QNH 1013.2 hPa."""
    mb = Mb().put(1, 1, 1).put(2, 12, round(selected_ft / 16))
    mb.put(27, 1, 1).put(28, 12, round((1013.2 - 800) / 0.1))
    mb.put(54, 1, 1).put(55, 2, 2)  # target altitude source: MCP/FCU selected altitude
    return mb.value.to_bytes(7, "big") + u8(0x40)


def bds60(heading, ias, mach, vrate):
    """Heading and speed report."""
    hdg = heading if heading <= 180 else heading - 360
    mb = Mb().put(1, 1, 1).put(2, 11, round(hdg / (90 / 512)))
    mb.put(13, 1, 1).put(14, 10, round(ias))
    mb.put(24, 1, 1).put(25, 10, round(mach / (2.048 / 512)))
    rate = round(vrate / 32)
    mb.put(35, 1, 1).put(36, 10, rate).put(46, 1, 1).put(47, 10, rate)
    return mb.value.to_bytes(7, "big") + u8(0x60)


# ------------------------------------------------------------------ scenario

class Target:
    def __init__(self, spec, track_number):
        (self.callsign, self.address, self.squawk, self.kind, self.x0, self.y0, self.track, self.gs,
         self.alt0, self.vrate, self.first, self.last) = spec
        self.track_number = track_number

    def state(self, t):
        """Position (NM), altitude (ft) and vertical rate at t seconds after START."""
        d = self.gs * t / 3600
        x = self.x0 + d * math.sin(math.radians(self.track))
        y = self.y0 + d * math.cos(math.radians(self.track))
        alt = None if self.alt0 is None else self.alt0 + self.vrate * t / 60
        if alt is not None and self.vrate:
            limit = 23000 if self.vrate > 0 else 11000  # climb to FL230, descend to FL110
            alt = min(alt, limit) if self.vrate > 0 else max(alt, limit)
        vrate = self.vrate if alt not in (23000, 11000) else 0
        return x, y, alt, vrate


def azimuth(x, y): return math.degrees(math.atan2(x, y)) % 360


def plot(target, scan, rng):
    """(time, azimuth, CAT048 record bytes, expected values) for one detection."""
    t = scan * PERIOD
    x, y, _, _ = target.state(t)
    t = (scan + azimuth(x, y) / 360) * PERIOD   # the antenna reaches the target a little later
    x, y, alt, vrate = target.state(t)
    theta = azimuth(x, y)
    ground = math.hypot(x, y)
    slant = math.hypot(ground, (alt or 0) / FT_PER_NM)
    rho_measured = slant + rng.gauss(0, 0.015)
    theta_measured = (theta + rng.gauss(0, 0.03)) % 360

    kind = target.kind
    ending = scan == target.last
    items = {1: bytes([SAC, SIC]), 2: time_of_day(START + t)}
    items[3] = u8((0 if ending else TYP[kind]) << 5)
    if not ending:
        items[4] = u16(round(rho_measured * 256)) + u16(round(theta_measured * 65536 / 360) % 65536)
        if kind != "P":
            items[5] = u16(target.squawk)
            fl = round(alt / (25 if kind != "AC" else 100)) * (25 if kind != "AC" else 100) / 100
            items[6] = u16(round(fl * 4) & 0x3FFF)
            srl = rng.randint(9, 14)
            srr = rng.randint(4, 9) if kind != "AC" else rng.randint(6, 12)
            sam = max(-95, min(-40, round(-48 - slant * 0.3 + rng.gauss(0, 2))))
            presence, fields = 0xE0, u8(srl) + u8(srr) + u8(sam)
            if kind == "SP":
                presence |= 0x18
                fields += u8(rng.randint(14, 22)) + u8(max(-100, round(-60 - slant * 0.25)))
            items[7] = u8(presence) + fields
        else:
            items[7] = u8(0x18) + u8(rng.randint(12, 20)) + u8(round(-68 - ground * 0.3))
        if kind in ("S", "SP"):
            items[9] = icao_chars(target.callsign)
            tas = target.gs
            mach = tas / (661.5 - 0.0019 * alt)
            ias = tas / (1 + alt / 1000 * 0.02)
            selected = 23000 if target.vrate > 0 else (11000 if target.vrate < 0 else alt)
            items[10] = u8(2) + bds40(round(selected / 100) * 100) + bds60(target.track, ias, mach, vrate)
        items[12] = u16(round(x * 128)) + u16(round(y * 128))
        items[13] = u16(round(target.gs / 3600 * 16384)) + u16(round(target.track * 65536 / 360) % 65536)
    if kind in ("S", "SP"):
        items[8] = u24(target.address)
    items[11] = u16(target.track_number)
    tentative = scan < target.first + 2 and target.first > 0
    cdm = 1 if vrate > 300 else (2 if vrate < -300 else 0)
    items[14] = u8((tentative << 7) | (RAD[kind] << 5) | (cdm << 1) | 1) + u8(0x80 if ending else 0x00)
    if kind in ("S", "SP") and not ending:
        stat = 2 if target.squawk == 0o7700 else 0
        items[21] = u16((1 << 13) | (stat << 10) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | 5)
    expected = None if ending else {
        "rho": rho_measured, "theta": theta_measured, "callsign": target.callsign,
        "squawk": target.squawk, "fl": None if kind == "P" else round(fl * 4) / 4,
    }
    return t, theta, record(items), expected


def north_marker(scan, counts):
    t = START + scan * PERIOD
    items = {
        1: bytes([SAC, SIC]), 2: u8(1), 3: time_of_day(t), 5: u16(round(PERIOD * 128)),
        6: u8(0x84) + u8(0x00) + u16((1 << 13) | (1 << 9)),   # I034/050: COM all nominal; MDS channel A, DLF channel B
        7: u8(0x84) + u8(0x00) + u8(0x00),                  # I034/060: no data reduction
        11: u16(612) + u24(round(47.4586 * 2 ** 23 / 180)) + u24(round(8.5503 * 2 ** 23 / 180)),
        12: u8(1) + u8(-2 & 0xFF),                          # I034/090 collimation error
    }
    if counts:
        items[8] = u8(len(counts)) + b"".join(u16((typ << 11) | min(n, 2047)) for typ, n in sorted(counts.items()))
    return record(items)


def sector_crossing(scan, sector):
    t = START + (scan + sector / 8) * PERIOD
    return record({1: bytes([SAC, SIC]), 2: u8(2), 3: time_of_day(t), 4: u8(sector * 32)})


def sensor_status(scan):
    t = START + scan * PERIOD + 0.25
    ok = record({
        1: bytes([SAC, SDPS_SIC]), 2: u8(1), 3: time_of_day(t), 4: bytes([SAC, SIC]),
        5: u8(0x01) + u8(0x00),                 # operational, all GO
        6: u16(-3), 7: u16(12) + u16(3), 8: u16(-9),
    })
    degraded = record({
        1: bytes([SAC, SDPS_SIC]), 2: u8(1), 3: time_of_day(t), 4: bytes([SAC, 43]),
        5: u8((1 << 6) | (1 << 5) | 1) + u8(0x00),  # degraded, PSR NOGO
        6: u16(5),
    })
    return block(63, [ok, degraded])


def generate(seed=48):
    rng = random.Random(seed)
    targets = [Target(spec, 1021 + 37 * i) for i, spec in enumerate(TRAFFIC)]
    datagrams, expectations = [], []
    counts = {}
    for scan in range(SCANS):
        datagrams.append(block(34, [north_marker(scan, counts)]))
        if scan % 5 == 0:
            datagrams.append(sensor_status(scan))
        counts = {}
        plots = []
        for target in targets:
            if target.first <= scan <= target.last:
                plots.append(plot(target, scan, rng))
                typ = TYP[target.kind]
                counts[typ] = counts.get(typ, 0) + 1
        for sector in range(8):
            in_sector = sorted((p for p in plots if sector * 45 <= p[1] < (sector + 1) * 45), key=lambda p: p[0])
            parts = []
            if in_sector:
                parts.append(block(48, [p[2] for p in in_sector]))
                expectations.extend(p[3] for p in in_sector)
            if sector < 7:
                parts.append(block(34, [sector_crossing(scan, sector + 1)]))
            if parts:
                datagrams.append(b"".join(parts))
    return datagrams, expectations


# ------------------------------------------------------------------ validation

def validate(datagrams, expectations):
    try:
        from asterix.base import Bits, RawDatablock
        import asterix.generated as gen
    except ImportError:
        print("libasterix not installed: skipping validation", file=sys.stderr)
        return
    specs = {34: gen.Cat_034_1_29, 48: gen.Cat_048_1_32, 63: gen.Cat_063_1_7}
    plots = []
    for d in datagrams:
        blocks = RawDatablock.parse(Bits.from_bytes(d))
        assert not isinstance(blocks, ValueError), blocks
        for db in blocks:
            records = specs[db.get_category()].cv_uap.parse(db.get_raw_records())
            assert not isinstance(records, ValueError), records
            if db.get_category() == 48:
                plots.extend(r for r in records if r.get_item("040") is not None)
    expected = [e for e in expectations if e is not None]
    assert len(plots) == len(expected), (len(plots), len(expected))
    for rec, exp in zip(plots, expected):
        i040 = rec.get_item("040").variation
        rho = i040.get_item("RHO").variation.content.as_quantity("NM")
        theta = i040.get_item("THETA").variation.content.as_quantity("°")
        assert abs(rho - exp["rho"]) <= 1 / 512, (rho, exp)
        assert min(abs(theta - exp["theta"]), 360 - abs(theta - exp["theta"])) <= 360 / 65536, (theta, exp)
        if exp["callsign"]:
            got = rec.get_item("240").variation.content.as_string().rstrip()
            assert got == exp["callsign"], (got, exp)
        if exp["squawk"] is not None:
            got = rec.get_item("070").variation.get_item("MODE3A").variation.content.as_string()
            assert got == format(exp["squawk"], "04o"), (got, exp)
        if exp["fl"] is not None:
            got = rec.get_item("090").variation.get_item("FL").variation.content.as_quantity("FL")
            assert got == exp["fl"], (got, exp)
    print(f"validated {len(datagrams)} datagrams, {len(plots)} plots with libasterix", file=sys.stderr)


def main():
    datagrams, expectations = generate()
    validate(datagrams, expectations)
    print("# Synthetic ASTERIX feed for the asterix demo (fictional traffic): one datagram per line.")
    print(f"# Radar {SAC}/{SIC}, {SCANS} scans of {PERIOD:.3f} s; CAT048 plots, CAT034 service messages, CAT063 status.")
    for d in datagrams:
        print(d.hex())


if __name__ == "__main__":
    main()
