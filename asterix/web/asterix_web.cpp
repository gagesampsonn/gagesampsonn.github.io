// C interface used by the browser demo (web/demo.html). Compiled with
// Emscripten into a JavaScript module; see web/build.py.

#include "capture.hpp"

#include <asterix/asterix.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define ASTERIX_WEB_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define ASTERIX_WEB_EXPORT
#endif

namespace {

using asterix::json::Writer;

// Records written out in full; further datagrams are still decoded and counted.
constexpr std::size_t kRecordBudget = 20000;

const char kHexDigits[] = "0123456789ABCDEF";

std::string to_hex(const std::uint8_t* p, std::size_t n) {
    std::string s;
    s.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        s += kHexDigits[p[i] >> 4];
        s += kHexDigits[p[i] & 0xF];
    }
    return s;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Same rules as asterix-dump: whitespace, the separators , : ; - and 0x prefixes are ignored.
bool parse_hex(std::string_view text, std::vector<std::uint8_t>& out, std::string& error) {
    out.clear();
    int high = -1;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X') && high < 0) {
            ++i;
            continue;
        }
        const int v = hex_value(c);
        if (v >= 0) {
            if (high < 0) {
                high = v;
            } else {
                out.push_back(static_cast<std::uint8_t>(high * 16 + v));
                high = -1;
            }
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ':' || c == ';' ||
                   c == '-') {
            continue;
        } else {
            error = std::string("unexpected character '") + c + "'";
            return false;
        }
    }
    if (high >= 0) {
        error = "odd number of hex digits";
        return false;
    }
    return true;
}

bool looks_like_text(const std::uint8_t* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t b = data[i];
        if (!(b == '\t' || b == '\n' || b == '\r' || (b >= 0x20 && b < 0x7F))) {
            return false;
        }
    }
    return size > 0;
}

struct Totals {
    std::size_t datagrams = 0;
    std::size_t blocks = 0;
    std::size_t records = 0;
    std::size_t errors = 0;
    std::size_t octets = 0;
    std::size_t written = 0;  // records written in full
    bool truncated = false;
};

void write_datagram(Writer& w, const std::vector<std::uint8_t>& bytes, std::size_t index,
                    const capture::Datagram* packet, Totals& totals) {
    const asterix::DecodeResult result = asterix::decode(bytes);
    ++totals.datagrams;
    totals.octets += bytes.size();
    totals.blocks += result.blocks.size();
    totals.errors += result.errors.size();
    std::size_t records = 0;
    for (const asterix::DataBlock& b : result.blocks) {
        records += b.records.size();
    }
    totals.records += records;
    if (totals.written >= kRecordBudget) {
        totals.truncated = true;
        return;
    }
    totals.written += records;

    w.begin_object();
    w.key("index").value(index);
    if (packet != nullptr) {
        w.key("time").value(packet->timestamp);
        w.key("src").value(packet->source);
        w.key("dst").value(packet->destination);
    }
    w.key("size").value(bytes.size());
    w.key("blocks").begin_array();
    for (const asterix::DataBlock& block : result.blocks) {
        w.begin_object();
        asterix::json::write_members(w, block);  // category, offset, length, [supported], records
        w.key("hex").value(to_hex(bytes.data() + block.offset, block.length));
        w.key("summaries").begin_array();
        for (const asterix::Record& r : block.records) {
            w.value(asterix::summarize(r));
        }
        w.end_array();
        w.end_object();
    }
    w.end_array();
    w.key("errors").begin_array();
    for (const asterix::DecodeError& e : result.errors) {
        asterix::json::write(w, e);
    }
    w.end_array();
    w.end_object();
}

// Calls fn(bytes, index, packet) for every datagram of the input, which may be
// a pcap/pcapng capture, hex text (one datagram per line) or raw data blocks.
// Returns the input format name.
template <class F>
const char* for_each_datagram(const std::uint8_t* data, std::size_t size, std::vector<std::string>& input_errors,
                              capture::Stats& stats, bool& is_capture, F&& fn) {
    is_capture = false;
    const capture::Format capture_format = capture::detect_format(data, size);
    if (capture_format != capture::Format::Unknown) {
        is_capture = true;
        std::vector<capture::Datagram> datagrams;
        try {
            datagrams = capture::read_udp_datagrams(data, size, stats);
        } catch (const capture::Error& e) {
            input_errors.emplace_back(e.what());
        }
        for (const capture::Datagram& d : datagrams) {
            fn(d.payload, d.packet, &d);
        }
        return capture_format == capture::Format::Pcap ? "pcap" : "pcapng";
    }
    if (looks_like_text(data, size)) {
        std::string_view text(reinterpret_cast<const char*>(data), size);
        std::size_t line_number = 0;
        while (!text.empty()) {
            const std::size_t eol = text.find('\n');
            std::string_view line = text.substr(0, eol);
            text = eol == std::string_view::npos ? std::string_view() : text.substr(eol + 1);
            ++line_number;
            const std::size_t comment = line.find('#');
            if (comment != std::string_view::npos) {
                line = line.substr(0, comment);
            }
            std::vector<std::uint8_t> bytes;
            std::string error;
            if (!parse_hex(line, bytes, error)) {
                input_errors.push_back("line " + std::to_string(line_number) + ": " + error);
                continue;
            }
            if (!bytes.empty()) {
                fn(bytes, line_number, nullptr);
            }
        }
        return "hex";
    }
    if (size > 0) {
        fn(std::vector<std::uint8_t>(data, data + size), 1, nullptr);
    }
    return "raw";
}

char* to_c_string(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p != nullptr) {
        std::memcpy(p, s.c_str(), s.size() + 1);
    }
    return p;
}

}  // namespace

extern "C" {

ASTERIX_WEB_EXPORT const char* asterix_web_version() {
    return asterix::version();
}

// Decodes input in any format asterix-dump accepts: pcap or pcapng captures,
// text with one hex-encoded datagram per line, or raw binary data blocks.
// Returns a JSON document (free it with asterix_web_free):
//   {"datagrams": [...], "format": ..., "capture": {...}, "inputErrors": [...], "totals": {...}}
ASTERIX_WEB_EXPORT char* asterix_web_decode(const std::uint8_t* data, std::size_t size) {
    std::string out;
    Writer w(out);
    Totals totals;
    std::vector<std::string> input_errors;
    capture::Stats stats;
    bool is_capture = false;

    w.begin_object();
    w.key("datagrams").begin_array();
    const char* format = for_each_datagram(data, size, input_errors, stats, is_capture,
        [&](const std::vector<std::uint8_t>& bytes, std::size_t index, const capture::Datagram* packet) {
            write_datagram(w, bytes, index, packet, totals);
        });
    w.end_array();
    w.key("format").value(format);
    if (is_capture) {
        w.key("capture").begin_object();
        w.key("packets").value(stats.packets);
        w.key("udp").value(stats.datagrams);
        w.key("fragments").value(stats.fragments);
        w.key("skipped").value(stats.skipped);
        w.end_object();
    }
    w.key("inputErrors").begin_array();
    for (const std::string& e : input_errors) {
        w.value(e);
    }
    w.end_array();
    w.key("totals").begin_object();
    w.key("datagrams").value(totals.datagrams);
    w.key("blocks").value(totals.blocks);
    w.key("records").value(totals.records);
    w.key("errors").value(totals.errors);
    w.key("octets").value(totals.octets);
    w.key("truncated").value(totals.truncated);
    w.end_object();
    w.end_object();
    return to_c_string(out);
}

// Decodes the input like asterix_web_decode but builds no output: the page
// uses it to time the decoder itself. Returns the number of records.
ASTERIX_WEB_EXPORT std::size_t asterix_web_count(const std::uint8_t* data, std::size_t size) {
    std::vector<std::string> input_errors;
    capture::Stats stats;
    bool is_capture = false;
    std::size_t records = 0;
    for_each_datagram(data, size, input_errors, stats, is_capture,
        [&](const std::vector<std::uint8_t>& bytes, std::size_t, const capture::Datagram*) {
            for (const asterix::DataBlock& b : asterix::decode(bytes).blocks) {
                records += b.records.size();
            }
        });
    return records;
}

ASTERIX_WEB_EXPORT std::uint8_t* asterix_web_alloc(std::size_t size) {
    return static_cast<std::uint8_t*>(std::malloc(size > 0 ? size : 1));
}

ASTERIX_WEB_EXPORT void asterix_web_free(void* p) {
    std::free(p);
}

}  // extern "C"
