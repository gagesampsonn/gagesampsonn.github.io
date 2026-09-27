#pragma once

// Extraction of UDP payloads from pcap and pcapng capture files, so that
// recorded ASTERIX traffic can be fed to the decoder.
//
// Supported link layers: Ethernet (with 802.1Q/802.1ad VLAN tags), Linux
// cooked capture v1/v2, BSD loopback and raw IPv4/IPv6. Fragmented IP
// datagrams are counted and skipped (no reassembly).

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace capture {

struct Datagram {
    std::size_t packet = 0;       ///< 1-based packet number in the capture file
    double timestamp = 0.0;       ///< seconds since the Unix epoch (0 if unknown)
    std::string source;           ///< "address:port"
    std::string destination;      ///< "address:port"
    std::uint16_t source_port = 0;
    std::uint16_t destination_port = 0;
    std::vector<std::uint8_t> payload;
};

struct Stats {
    std::size_t packets = 0;    ///< packets in the capture
    std::size_t datagrams = 0;  ///< UDP datagrams extracted
    std::size_t fragments = 0;  ///< fragmented IP packets skipped (no reassembly)
    std::size_t skipped = 0;    ///< other packets skipped (not UDP, unsupported link layer, malformed)
};

enum class Format { Unknown, Pcap, PcapNg };

/// Recognises pcap and pcapng files by their magic number.
Format detect_format(const std::uint8_t* data, std::size_t size) noexcept;

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Parses a pcap or pcapng file held in memory and returns its UDP datagrams.
/// Throws capture::Error when the file structure is invalid. A packet cut off
/// by the end of the file (e.g. a capture that is still being written) ends
/// the parse without an error.
std::vector<Datagram> read_udp_datagrams(const std::uint8_t* data, std::size_t size, Stats& stats);

}  // namespace capture
