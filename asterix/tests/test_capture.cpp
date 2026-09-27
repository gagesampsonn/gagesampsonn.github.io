#include "capture.hpp"
#include "helpers.hpp"

#include <array>

using namespace test;

namespace {

void put16be(Bytes& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v));
}

void put16(Bytes& b, unsigned v, bool little_endian) {
    if (little_endian) {
        b.push_back(static_cast<std::uint8_t>(v));
        b.push_back(static_cast<std::uint8_t>(v >> 8));
    } else {
        put16be(b, v);
    }
}

void put32(Bytes& b, std::uint32_t v, bool little_endian) {
    for (int i = 0; i < 4; ++i) {
        const int shift = little_endian ? 8 * i : 24 - 8 * i;
        b.push_back(static_cast<std::uint8_t>(v >> shift));
    }
}

Bytes udp(const Bytes& payload, unsigned source_port, unsigned destination_port) {
    Bytes b;
    put16be(b, source_port);
    put16be(b, destination_port);
    put16be(b, static_cast<unsigned>(payload.size() + 8));
    put16be(b, 0);
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}

Bytes ipv4(const Bytes& l4, std::array<std::uint8_t, 4> src, std::array<std::uint8_t, 4> dst,
           unsigned fragment = 0, std::uint8_t protocol = 17) {
    Bytes b{0x45, 0x00};
    put16be(b, static_cast<unsigned>(l4.size() + 20));
    put16be(b, 0x1234);
    put16be(b, fragment);
    b.push_back(64);
    b.push_back(protocol);
    put16be(b, 0);
    b.insert(b.end(), src.begin(), src.end());
    b.insert(b.end(), dst.begin(), dst.end());
    b.insert(b.end(), l4.begin(), l4.end());
    return b;
}

Bytes ipv6_with_hop_by_hop(const Bytes& l4) {
    Bytes b{0x60, 0x00, 0x00, 0x00};
    put16be(b, static_cast<unsigned>(l4.size() + 8));
    b.push_back(0);   // next header: hop-by-hop options
    b.push_back(64);  // hop limit
    const Bytes src = hex("2001 0db8 0000 0000 0000 0000 0000 0001");
    const Bytes dst = hex("ff15 0000 0000 0000 0000 0000 0000 0048");
    b.insert(b.end(), src.begin(), src.end());
    b.insert(b.end(), dst.begin(), dst.end());
    const Bytes hop = hex("11 00 01 04 00 00 00 00");  // next header UDP, 8 octets, PadN
    b.insert(b.end(), hop.begin(), hop.end());
    b.insert(b.end(), l4.begin(), l4.end());
    return b;
}

Bytes ethernet(const Bytes& l3, unsigned ethertype, bool vlan = false) {
    Bytes b = hex("01 00 5e 01 01 01 00 11 22 33 44 55");
    if (vlan) {
        put16be(b, 0x8100);
        put16be(b, 100);
    }
    put16be(b, ethertype);
    b.insert(b.end(), l3.begin(), l3.end());
    return b;
}

Bytes pcap_file(std::uint32_t magic, bool little_endian, std::uint32_t link_type,
                const std::vector<std::pair<std::uint32_t, Bytes>>& packets, std::uint32_t fraction = 0) {
    Bytes b;
    put32(b, magic, little_endian);
    put16(b, 2, little_endian);
    put16(b, 4, little_endian);
    put32(b, 0, little_endian);
    put32(b, 0, little_endian);
    put32(b, 65535, little_endian);
    put32(b, link_type, little_endian);
    for (const auto& [seconds, data] : packets) {
        put32(b, seconds, little_endian);
        put32(b, fraction, little_endian);
        put32(b, static_cast<std::uint32_t>(data.size()), little_endian);
        put32(b, static_cast<std::uint32_t>(data.size()), little_endian);
        b.insert(b.end(), data.begin(), data.end());
    }
    return b;
}

// pcapng block with the body padded to 32 bits and both length fields.
Bytes pcapng_block(std::uint32_t type, const Bytes& body, bool little_endian) {
    Bytes padded = body;
    while (padded.size() % 4 != 0) padded.push_back(0);
    const auto length = static_cast<std::uint32_t>(padded.size() + 12);
    Bytes b;
    put32(b, type, little_endian);
    put32(b, length, little_endian);
    b.insert(b.end(), padded.begin(), padded.end());
    put32(b, length, little_endian);
    return b;
}

Bytes pcapng_section(bool little_endian) {
    Bytes body;
    put32(body, 0x1A2B3C4Du, little_endian);
    put16(body, 1, little_endian);
    put16(body, 0, little_endian);
    put32(body, 0xFFFFFFFFu, little_endian);  // section length unknown
    put32(body, 0xFFFFFFFFu, little_endian);
    return pcapng_block(0x0A0D0D0Au, body, little_endian);
}

Bytes pcapng_interface(unsigned link_type, int tsresol, bool little_endian) {
    Bytes body;
    put16(body, link_type, little_endian);
    put16(body, 0, little_endian);
    put32(body, 0, little_endian);  // no snap length limit
    if (tsresol >= 0) {
        put16(body, 9, little_endian);
        put16(body, 1, little_endian);
        body.insert(body.end(), {static_cast<std::uint8_t>(tsresol), 0, 0, 0});
        put16(body, 0, little_endian);  // opt_endofopt
        put16(body, 0, little_endian);
    }
    return pcapng_block(1, body, little_endian);
}

Bytes pcapng_packet(std::uint32_t interface_id, std::uint64_t ts, const Bytes& data, bool little_endian) {
    Bytes body;
    put32(body, interface_id, little_endian);
    put32(body, static_cast<std::uint32_t>(ts >> 32), little_endian);
    put32(body, static_cast<std::uint32_t>(ts), little_endian);
    put32(body, static_cast<std::uint32_t>(data.size()), little_endian);
    put32(body, static_cast<std::uint32_t>(data.size()), little_endian);
    body.insert(body.end(), data.begin(), data.end());
    return pcapng_block(6, body, little_endian);
}

std::vector<capture::Datagram> read(const Bytes& file, capture::Stats& stats) {
    return capture::read_udp_datagrams(file.data(), file.size(), stats);
}

}  // namespace

TEST(capture_pcap_ethernet) {
    const Bytes plot = cat048_example();
    const Bytes service = cat034_example();
    const Bytes file = pcap_file(0xA1B2C3D4u, true, 1, {
        {1700000000u, ethernet(ipv4(udp(plot, 5000, 8600), {10, 1, 2, 3}, {232, 1, 1, 1}), 0x0800)},
        {1700000001u, ethernet(hex("00 01 08 00 06 04 00 01"), 0x0806)},  // ARP
        {1700000002u, ethernet(ipv4(udp(plot, 5000, 8600), {10, 1, 2, 3}, {232, 1, 1, 1}, 0x2000), 0x0800)},
        {1700000003u, ethernet(ipv4(udp(service, 5001, 8601), {10, 1, 2, 4}, {232, 1, 1, 2}), 0x0800, true)},
        {1700000004u, ethernet(ipv4(hex("00 00 00 00"), {10, 1, 2, 3}, {10, 1, 2, 4}, 0, 6), 0x0800)},  // TCP
    }, 250000);
    CHECK(capture::detect_format(file.data(), file.size()) == capture::Format::Pcap);

    capture::Stats stats;
    const auto datagrams = read(file, stats);
    CHECK_EQ(stats.packets, 5u);
    CHECK_EQ(stats.datagrams, 2u);
    CHECK_EQ(stats.fragments, 1u);
    CHECK_EQ(stats.skipped, 2u);
    REQUIRE(datagrams.size() == 2);

    CHECK_EQ(datagrams[0].packet, 1u);
    CHECK_EQ(datagrams[0].timestamp, 1700000000.25);
    CHECK_EQ(datagrams[0].source, "10.1.2.3:5000");
    CHECK_EQ(datagrams[0].destination, "232.1.1.1:8600");
    CHECK_EQ(datagrams[0].source_port, 5000);
    CHECK_EQ(datagrams[0].destination_port, 8600);
    CHECK(datagrams[0].payload == plot);

    CHECK_EQ(datagrams[1].packet, 4u);
    CHECK_EQ(datagrams[1].destination, "232.1.1.2:8601");
    CHECK(datagrams[1].payload == service);
    CHECK(asterix::decode(datagrams[1].payload).ok());
}

TEST(capture_pcap_big_endian_nanoseconds_raw_ip) {
    const Bytes file = pcap_file(0xA1B23C4Du, false, 101, {
        {100u, ipv4(udp(cat034_example(), 1, 2), {192, 168, 0, 1}, {192, 168, 0, 2})},
    }, 500000000);
    capture::Stats stats;
    const auto datagrams = read(file, stats);
    REQUIRE(datagrams.size() == 1);
    CHECK_EQ(datagrams[0].timestamp, 100.5);
    CHECK_EQ(datagrams[0].source, "192.168.0.1:1");
    CHECK(datagrams[0].payload == cat034_example());
}

TEST(capture_pcap_other_link_layers) {
    const Bytes ip = ipv4(udp(hex("30 00 03"), 7, 8), {1, 2, 3, 4}, {5, 6, 7, 8});
    Bytes sll = hex("00 00 00 01 00 06 00 11 22 33 44 55 00 00");
    put16be(sll, 0x0800);
    sll.insert(sll.end(), ip.begin(), ip.end());
    Bytes sll2;
    put16be(sll2, 0x0800);
    const Bytes sll2_rest = hex("00 00 00 00 00 02 00 01 04 06 00 11 22 33 44 55 00 00");
    sll2.insert(sll2.end(), sll2_rest.begin(), sll2_rest.end());
    sll2.insert(sll2.end(), ip.begin(), ip.end());
    Bytes loopback = hex("02 00 00 00");
    loopback.insert(loopback.end(), ip.begin(), ip.end());

    for (const auto& [link_type, frame] : std::vector<std::pair<std::uint32_t, Bytes>>{
             {113, sll}, {276, sll2}, {0, loopback}, {228, ip}}) {
        capture::Stats stats;
        const auto datagrams = read(pcap_file(0xA1B2C3D4u, true, link_type, {{1u, frame}}), stats);
        REQUIRE(datagrams.size() == 1);
        CHECK_EQ(datagrams[0].source, "1.2.3.4:7");
        CHECK(datagrams[0].payload == hex("30 00 03"));
    }
}

TEST(capture_pcap_truncated_final_packet_is_ignored) {
    Bytes file = pcap_file(0xA1B2C3D4u, true, 101, {
        {1u, ipv4(udp(cat048_example(), 1, 2), {1, 1, 1, 1}, {2, 2, 2, 2})},
        {2u, ipv4(udp(cat048_example(), 1, 2), {1, 1, 1, 1}, {2, 2, 2, 2})},
    });
    file.resize(file.size() - 10);
    capture::Stats stats;
    CHECK_EQ(read(file, stats).size(), 1u);
}

TEST(capture_pcapng_little_endian_ipv6) {
    const Bytes frame = ethernet(ipv6_with_hop_by_hop(udp(cat048_example(), 1234, 8600)), 0x86DD);
    const Bytes file = concat({pcapng_section(true), pcapng_interface(1, 9, true),
                               pcapng_packet(0, 1700000000500000000ull, frame, true)});
    CHECK(capture::detect_format(file.data(), file.size()) == capture::Format::PcapNg);
    capture::Stats stats;
    const auto datagrams = read(file, stats);
    REQUIRE(datagrams.size() == 1);
    CHECK_EQ(datagrams[0].source, "[2001:db8::1]:1234");
    CHECK_EQ(datagrams[0].destination, "[ff15::48]:8600");
    CHECK_NEAR(datagrams[0].timestamp, 1700000000.5, 1e-6);
    CHECK(datagrams[0].payload == cat048_example());
}

TEST(capture_pcapng_big_endian_and_simple_packets) {
    const Bytes frame = ethernet(ipv4(udp(cat034_example(), 3, 4), {9, 9, 9, 9}, {8, 8, 8, 8}), 0x0800);
    Bytes simple_body;
    put32(simple_body, static_cast<std::uint32_t>(frame.size()), false);
    simple_body.insert(simple_body.end(), frame.begin(), frame.end());
    const Bytes file = concat({pcapng_section(false), pcapng_interface(1, -1, false),
                               pcapng_packet(0, 3000000ull, frame, false), pcapng_block(3, simple_body, false),
                               pcapng_block(5, hex("00 00 00 00"), false)});  // interface statistics: skipped
    capture::Stats stats;
    const auto datagrams = read(file, stats);
    REQUIRE(datagrams.size() == 2);
    CHECK_EQ(datagrams[0].timestamp, 3.0);  // default resolution: microseconds
    CHECK_EQ(datagrams[0].source, "9.9.9.9:3");
    CHECK(datagrams[1].payload == cat034_example());
    CHECK_EQ(datagrams[1].packet, 2u);
}

TEST(capture_invalid_files) {
    capture::Stats stats;
    const Bytes junk = hex("30 00 30 fd f7");
    bool threw = false;
    try {
        read(junk, stats);
    } catch (const capture::Error&) {
        threw = true;
    }
    CHECK(threw);

    Bytes bad_block = pcapng_section(true);
    const Bytes tail = hex("06 00 00 00 0d 00 00 00 00 00 00 00 00 00 00 00");  // length 13
    bad_block.insert(bad_block.end(), tail.begin(), tail.end());
    threw = false;
    try {
        read(bad_block, stats);
    } catch (const capture::Error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(capture::detect_format(junk.data(), 2) == capture::Format::Unknown);
}
