#include "capture.hpp"

#include <cmath>
#include <cstdio>

namespace capture {

namespace {

std::uint16_t load16(const std::uint8_t* p, bool little_endian) {
    return little_endian ? static_cast<std::uint16_t>(p[0] | (p[1] << 8))
                         : static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::uint32_t load32(const std::uint8_t* p, bool little_endian) {
    const std::uint32_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3];
    return little_endian ? (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24))
                         : ((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
}

std::uint16_t net16(const std::uint8_t* p) {
    return load16(p, false);
}

std::string ipv4_text(const std::uint8_t* a) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return buf;
}

// RFC 5952 text form: lower-case hex groups, longest zero run compressed.
std::string ipv6_text(const std::uint8_t* a) {
    unsigned groups[8];
    for (int i = 0; i < 8; ++i) {
        groups[i] = net16(a + 2 * i);
    }
    int best_start = -1, best_len = 0;
    for (int i = 0; i < 8;) {
        if (groups[i] != 0) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 8 && groups[j] == 0) {
            ++j;
        }
        if (j - i > best_len && j - i >= 2) {
            best_start = i;
            best_len = j - i;
        }
        i = j;
    }
    std::string out;
    char buf[8];
    for (int i = 0; i < 8; ++i) {
        if (i == best_start) {
            out += "::";
            i += best_len - 1;
            continue;
        }
        if (!out.empty() && out.back() != ':') {
            out += ':';
        }
        std::snprintf(buf, sizeof buf, "%x", groups[i]);
        out += buf;
    }
    return out;
}

std::string endpoint(const std::string& address, bool ipv6, std::uint16_t port) {
    return (ipv6 ? "[" + address + "]" : address) + ":" + std::to_string(port);
}

struct PacketContext {
    std::size_t number;
    double timestamp;
};

class Extractor {
public:
    Extractor(std::vector<Datagram>& out, Stats& stats) : out_(out), stats_(stats) {}

    void packet(std::uint32_t link_type, const std::uint8_t* p, std::size_t n, const PacketContext& ctx) {
        ++stats_.packets;
        if (!link_layer(link_type, p, n, ctx)) {
            ++stats_.skipped;
        }
    }

private:
    bool link_layer(std::uint32_t link_type, const std::uint8_t* p, std::size_t n, const PacketContext& ctx) {
        switch (link_type) {
            case 0:  // BSD loopback: address family in the capturing host's byte order
            case 108: {  // OpenBSD loopback: address family in network byte order
                if (n < 4) return false;
                const std::uint32_t family = link_type == 108 ? load32(p, false)
                                                              : (p[0] != 0 ? load32(p, true) : load32(p, false));
                if (family == 2) return ipv4(p + 4, n - 4, ctx);
                if (family == 10 || family == 24 || family == 28 || family == 30) return ipv6(p + 4, n - 4, ctx);
                return false;
            }
            case 1:  // Ethernet
                if (n < 14) return false;
                return ethertype(net16(p + 12), p + 14, n - 14, ctx);
            case 12:
            case 14:
            case 101:  // raw IP
                if (n < 1) return false;
                return (p[0] >> 4) == 4 ? ipv4(p, n, ctx) : ipv6(p, n, ctx);
            case 113:  // Linux cooked capture v1
                if (n < 16) return false;
                return ethertype(net16(p + 14), p + 16, n - 16, ctx);
            case 228:  // raw IPv4
                return ipv4(p, n, ctx);
            case 229:  // raw IPv6
                return ipv6(p, n, ctx);
            case 276:  // Linux cooked capture v2
                if (n < 20) return false;
                return ethertype(net16(p), p + 20, n - 20, ctx);
            default:
                return false;
        }
    }

    bool ethertype(std::uint16_t type, const std::uint8_t* p, std::size_t n, const PacketContext& ctx) {
        // Skip 802.1Q / 802.1ad / QinQ VLAN tags.
        while (type == 0x8100 || type == 0x88A8 || type == 0x9100) {
            if (n < 4) return false;
            type = net16(p + 2);
            p += 4;
            n -= 4;
        }
        if (type == 0x0800) return ipv4(p, n, ctx);
        if (type == 0x86DD) return ipv6(p, n, ctx);
        return false;
    }

    bool ipv4(const std::uint8_t* p, std::size_t n, const PacketContext& ctx) {
        if (n < 20 || (p[0] >> 4) != 4) return false;
        const std::size_t header = std::size_t{p[0] & 0x0Fu} * 4;
        std::size_t total = net16(p + 2);
        if (header < 20 || total < header) return false;
        if (total > n) total = n;  // truncated by the snapshot length
        const std::uint16_t fragment = net16(p + 6);
        if ((fragment & 0x3FFFu) != 0) {  // more-fragments flag or non-zero offset
            ++stats_.fragments;
            return true;
        }
        if (p[9] != 17) return false;
        return udp(p + header, total - header, ipv4_text(p + 12), ipv4_text(p + 16), false, ctx);
    }

    bool ipv6(const std::uint8_t* p, std::size_t n, const PacketContext& ctx) {
        if (n < 40 || (p[0] >> 4) != 6) return false;
        std::size_t end = 40 + std::size_t{net16(p + 4)};
        if (end > n) end = n;
        std::uint8_t next = p[6];
        std::size_t pos = 40;
        // Hop-by-hop, routing and destination options headers.
        while (next == 0 || next == 43 || next == 60) {
            if (end - pos < 8) return false;
            const std::size_t len = (std::size_t{p[pos + 1]} + 1) * 8;
            if (end - pos < len) return false;
            next = p[pos];
            pos += len;
        }
        if (next == 44) {
            ++stats_.fragments;
            return true;
        }
        if (next != 17) return false;
        return udp(p + pos, end - pos, ipv6_text(p + 8), ipv6_text(p + 24), true, ctx);
    }

    bool udp(const std::uint8_t* p, std::size_t n, const std::string& src, const std::string& dst, bool v6,
             const PacketContext& ctx) {
        if (n < 8) return false;
        std::size_t length = net16(p + 4);
        if (length == 0 || length > n) length = n;  // jumbogram or truncated capture
        if (length < 8) return false;
        Datagram d;
        d.packet = ctx.number;
        d.timestamp = ctx.timestamp;
        d.source_port = net16(p);
        d.destination_port = net16(p + 2);
        d.source = endpoint(src, v6, d.source_port);
        d.destination = endpoint(dst, v6, d.destination_port);
        d.payload.assign(p + 8, p + length);
        out_.push_back(std::move(d));
        ++stats_.datagrams;
        return true;
    }

    std::vector<Datagram>& out_;
    Stats& stats_;
};

constexpr std::size_t kMaxPacket = 256u * 1024u * 1024u;

void read_pcap(const std::uint8_t* data, std::size_t size, Extractor& extractor) {
    if (size < 24) throw Error("pcap file header is truncated");
    const std::uint32_t magic = load32(data, true);
    bool little_endian = true;
    double fraction_scale = 1e-6;
    switch (magic) {
        case 0xA1B2C3D4u: break;
        case 0xD4C3B2A1u: little_endian = false; break;
        case 0xA1B23C4Du: fraction_scale = 1e-9; break;
        case 0x4D3CB2A1u: little_endian = false; fraction_scale = 1e-9; break;
        default: throw Error("not a pcap file");
    }
    const std::uint32_t link_type = load32(data + 20, little_endian) & 0xFFFFu;

    std::size_t pos = 24;
    std::size_t number = 0;
    while (size - pos >= 16) {
        const std::uint32_t seconds = load32(data + pos, little_endian);
        const std::uint32_t fraction = load32(data + pos + 4, little_endian);
        const std::uint32_t captured = load32(data + pos + 8, little_endian);
        if (captured > kMaxPacket) throw Error("pcap record length " + std::to_string(captured) + " is implausible");
        pos += 16;
        if (size - pos < captured) break;  // final packet cut off
        extractor.packet(link_type, data + pos, captured, {++number, seconds + fraction * fraction_scale});
        pos += captured;
    }
}

struct Interface {
    std::uint32_t link_type = 0;
    std::uint32_t snap_length = 0;
    double tick = 1e-6;  // seconds per timestamp unit
    double offset = 0;   // seconds added to every timestamp
};

void read_pcapng(const std::uint8_t* data, std::size_t size, Extractor& extractor) {
    bool little_endian = true;
    bool have_section = false;
    std::vector<Interface> interfaces;
    std::size_t number = 0;
    std::size_t pos = 0;

    while (size - pos >= 12) {
        const std::uint8_t* block = data + pos;
        if (load32(block, true) == 0x0A0D0D0Au) {  // section header, byte order independent
            const std::uint32_t order = load32(block + 8, true);
            if (order == 0x1A2B3C4Du) {
                little_endian = true;
            } else if (order == 0x4D3C2B1Au) {
                little_endian = false;
            } else {
                throw Error("pcapng section header has an invalid byte-order magic");
            }
            have_section = true;
            interfaces.clear();
        } else if (!have_section) {
            throw Error("not a pcapng file");
        }

        const std::uint32_t type = load32(block, little_endian);
        const std::uint32_t length = load32(block + 4, little_endian);
        if (length < 12 || length % 4 != 0) throw Error("pcapng block has an invalid length");
        if (length > size - pos) break;  // final block cut off
        const std::size_t body_end = length - 4;  // relative to block start, excludes trailing length

        if (type == 1 && body_end >= 16) {  // interface description
            Interface ifc;
            ifc.link_type = load16(block + 8, little_endian);
            ifc.snap_length = load32(block + 12, little_endian);
            std::size_t opt = 16;
            while (body_end - opt >= 4) {
                const std::uint16_t code = load16(block + opt, little_endian);
                const std::uint16_t len = load16(block + opt + 2, little_endian);
                opt += 4;
                if (code == 0 || len > body_end - opt) break;
                if (code == 9 && len >= 1) {  // if_tsresol
                    const std::uint8_t v = block[opt];
                    const double exponent = v & 0x7F;
                    ifc.tick = (v & 0x80) ? std::pow(2.0, -exponent) : std::pow(10.0, -exponent);
                } else if (code == 14 && len >= 8) {  // if_tsoffset: signed 64-bit seconds
                    const std::uint64_t first = load32(block + opt, little_endian);
                    const std::uint64_t second = load32(block + opt + 4, little_endian);
                    const std::uint64_t value = little_endian ? (second << 32) | first : (first << 32) | second;
                    ifc.offset = static_cast<double>(static_cast<std::int64_t>(value));
                }
                opt += (std::size_t{len} + 3) & ~std::size_t{3};
            }
            interfaces.push_back(ifc);
        } else if ((type == 6 || type == 2) && body_end >= 28) {  // enhanced / obsolete packet block
            const std::uint32_t id = type == 6 ? load32(block + 8, little_endian) : load16(block + 8, little_endian);
            const std::uint64_t high = load32(block + 12, little_endian);
            const std::uint64_t low = load32(block + 16, little_endian);
            const std::uint32_t captured = load32(block + 20, little_endian);
            if (id >= interfaces.size()) throw Error("pcapng packet refers to an undefined interface");
            if (captured > body_end - 28) throw Error("pcapng packet data exceeds its block");
            const Interface& ifc = interfaces[id];
            const double ts = static_cast<double>((high << 32) | low) * ifc.tick + ifc.offset;
            extractor.packet(ifc.link_type, block + 28, captured, {++number, ts});
        } else if (type == 3 && body_end >= 12) {  // simple packet block
            if (interfaces.empty()) throw Error("pcapng packet refers to an undefined interface");
            const Interface& ifc = interfaces[0];
            std::size_t captured = load32(block + 8, little_endian);
            if (captured > body_end - 12) captured = body_end - 12;
            if (ifc.snap_length != 0 && captured > ifc.snap_length) captured = ifc.snap_length;
            extractor.packet(ifc.link_type, block + 12, captured, {++number, 0.0});
        }
        pos += length;
    }
}

}  // namespace

Format detect_format(const std::uint8_t* data, std::size_t size) noexcept {
    if (size < 4) {
        return Format::Unknown;
    }
    const std::uint32_t magic = load32(data, true);
    if (magic == 0xA1B2C3D4u || magic == 0xD4C3B2A1u || magic == 0xA1B23C4Du || magic == 0x4D3CB2A1u) {
        return Format::Pcap;
    }
    if (magic == 0x0A0D0D0Au) {
        return Format::PcapNg;
    }
    return Format::Unknown;
}

std::vector<Datagram> read_udp_datagrams(const std::uint8_t* data, std::size_t size, Stats& stats) {
    std::vector<Datagram> out;
    Extractor extractor(out, stats);
    switch (detect_format(data, size)) {
        case Format::Pcap: read_pcap(data, size, extractor); break;
        case Format::PcapNg: read_pcapng(data, size, extractor); break;
        case Format::Unknown: throw Error("not a pcap or pcapng file");
    }
    return out;
}

}  // namespace capture
