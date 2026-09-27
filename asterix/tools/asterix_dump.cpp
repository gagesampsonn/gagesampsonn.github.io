// asterix-dump: decode ASTERIX CAT034 / CAT048 / CAT063 from capture files,
// raw binary recordings or hex text, and print records as text or JSON.

#include "capture.hpp"

#include <asterix/asterix.hpp>

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

const char kUsage[] = R"(Usage: asterix-dump [OPTIONS] [FILE...]

Decodes EUROCONTROL ASTERIX data: categories 034, 048 and 063.

FILE may be a pcap/pcapng capture (UDP payloads are decoded), a raw binary
file of concatenated data blocks, or a text file holding one hex-encoded
datagram per line ('#' starts a comment). Without FILE, or for '-', standard
input is read.

Input:
  -f, --format FMT    input format: auto (default), raw, hex or pcap
  -x, --hex HEX       decode HEX, e.g. "22 00 0b f0 19 0e 02 ...", instead of files
  -p, --port PORT     captures: only UDP datagrams to or from PORT

Output:
  -j, --json          JSON Lines: one object per data block, one per error
      --pretty        indented JSON (implies --json)
  -c, --category CAT  only show data blocks of category CAT (repeatable)
  -q, --quiet         no per-record output (combine with --stats)
  -s, --stats         print statistics to stderr when done
  -h, --help          show this help and exit
  -V, --version       show the version and exit

Exit status: 0 when everything decoded cleanly, 1 when decode errors were
found, 2 on usage or I/O errors.
)";

enum class InputFormat { Auto, Raw, Hex, Pcap };

struct Options {
    InputFormat format = InputFormat::Auto;
    bool json = false;
    int indent = -1;
    bool quiet = false;
    bool stats = false;
    std::optional<std::uint16_t> port;
    std::set<unsigned> categories;
    std::vector<std::string> hex_arguments;
    std::vector<std::string> files;
};

struct Statistics {
    std::size_t datagrams = 0;
    std::size_t octets = 0;
    std::map<unsigned, std::size_t> blocks;
    std::map<unsigned, std::size_t> records;
    std::size_t errors = 0;
    capture::Stats capture;
    bool had_capture = false;
};

// Where a datagram came from, for output context.
struct Context {
    std::string source;                         // file name, "-" or "--hex"
    std::size_t line = 0;                       // hex input: 1-based line number
    const capture::Datagram* packet = nullptr;  // capture input
};

[[noreturn]] void usage_error(const std::string& message) {
    std::fprintf(stderr, "asterix-dump: %s\nTry 'asterix-dump --help'.\n", message.c_str());
    std::exit(2);
}

unsigned long parse_number(const std::string& text, unsigned long max, const char* what) {
    char* end = nullptr;
    errno = 0;
    const unsigned long v = std::strtoul(text.c_str(), &end, 10);
    if (text.empty() || *end != '\0' || errno != 0 || v > max) {
        usage_error(std::string("invalid ") + what + ": '" + text + "'");
    }
    return v;
}

Options parse_options(int argc, char** argv) {
    Options opt;
    bool no_more_options = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage_error("option '" + arg + "' needs a value");
            }
            return argv[++i];
        };
        if (no_more_options || arg == "-" || arg.empty() || arg[0] != '-') {
            opt.files.push_back(arg);
        } else if (arg == "--") {
            no_more_options = true;
        } else if (arg == "-h" || arg == "--help") {
            std::fputs(kUsage, stdout);
            std::exit(0);
        } else if (arg == "-V" || arg == "--version") {
            std::printf("asterix-dump %s\n", asterix::version());
            std::exit(0);
        } else if (arg == "-f" || arg == "--format") {
            const std::string f = next();
            if (f == "auto") opt.format = InputFormat::Auto;
            else if (f == "raw") opt.format = InputFormat::Raw;
            else if (f == "hex") opt.format = InputFormat::Hex;
            else if (f == "pcap" || f == "pcapng") opt.format = InputFormat::Pcap;
            else usage_error("unknown input format '" + f + "'");
        } else if (arg == "-x" || arg == "--hex") {
            opt.hex_arguments.push_back(next());
        } else if (arg == "-p" || arg == "--port") {
            opt.port = static_cast<std::uint16_t>(parse_number(next(), 65535, "port"));
        } else if (arg == "-j" || arg == "--json") {
            opt.json = true;
        } else if (arg == "--pretty") {
            opt.json = true;
            opt.indent = 2;
        } else if (arg == "-c" || arg == "--category") {
            opt.categories.insert(static_cast<unsigned>(parse_number(next(), 255, "category")));
        } else if (arg == "-q" || arg == "--quiet") {
            opt.quiet = true;
        } else if (arg == "-s" || arg == "--stats") {
            opt.stats = true;
        } else {
            usage_error("unknown option '" + arg + "'");
        }
    }
    if (opt.files.empty() && opt.hex_arguments.empty()) {
        opt.files.push_back("-");
    }
    return opt;
}

bool read_file(const std::string& name, std::vector<std::uint8_t>& out) {
    if (name == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        out.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
        return !std::cin.bad();
    }
    std::ifstream in(name, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Parses hex digits, ignoring whitespace, the separators , : ; - and 0x prefixes.
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

bool looks_like_text(const std::vector<std::uint8_t>& data) {
    for (const std::uint8_t b : data) {
        if (!(b == '\t' || b == '\n' || b == '\r' || (b >= 0x20 && b < 0x7F))) {
            return false;
        }
    }
    return true;
}

void civil_from_days(long long z, long long& year, unsigned& month, unsigned& day) {
    // Howard Hinnant's algorithm, proleptic Gregorian calendar.
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = static_cast<long long>(yoe) + era * 400 + (month <= 2 ? 1 : 0);
}

std::string iso_time(double t) {
    if (!(t > 0) || t > 253402300799.0) {
        return "-";
    }
    auto seconds = static_cast<long long>(std::floor(t));
    auto micros = static_cast<long long>(std::llround((t - static_cast<double>(seconds)) * 1e6));
    if (micros >= 1000000) {
        ++seconds;
        micros -= 1000000;
    }
    long long year = 0;
    unsigned month = 0, day = 0;
    civil_from_days(seconds / 86400, year, month, day);
    const long long rem = seconds % 86400;
    char buf[96];
    std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02lld:%02lld:%02lld.%06lldZ", year, month, day, rem / 3600,
                  rem / 60 % 60, rem % 60, micros);
    return buf;
}

std::string where(const Context& ctx) {
    std::string s = ctx.source;
    if (ctx.packet != nullptr) {
        s += ": packet " + std::to_string(ctx.packet->packet);
    } else if (ctx.line != 0) {
        s += ": line " + std::to_string(ctx.line);
    }
    return s;
}

void context_members(asterix::json::Writer& w, const Context& ctx) {
    if (ctx.packet != nullptr) {
        w.key("packet").value(ctx.packet->packet);
        w.key("time").value(ctx.packet->timestamp);
        w.key("src").value(ctx.packet->source);
        w.key("dst").value(ctx.packet->destination);
    } else if (ctx.line != 0) {
        w.key("line").value(ctx.line);
    }
}

void process(const std::vector<std::uint8_t>& data, const Context& ctx, const Options& opt, Statistics& stats) {
    const asterix::DecodeResult result = asterix::decode(data);
    ++stats.datagrams;
    stats.octets += data.size();

    bool header_printed = false;
    for (const asterix::DataBlock& block : result.blocks) {
        ++stats.blocks[block.category];
        stats.records[block.category] += block.records.size();
        if (opt.quiet || (!opt.categories.empty() && opt.categories.count(block.category) == 0)) {
            continue;
        }
        if (opt.json) {
            std::string line;
            asterix::json::Writer w(line, opt.indent);
            w.begin_object();
            context_members(w, ctx);
            asterix::json::write_members(w, block);
            w.end_object();
            line += '\n';
            std::fputs(line.c_str(), stdout);
            continue;
        }
        const char* indent = "";
        if (ctx.packet != nullptr) {
            indent = "  ";
            if (!header_printed) {
                std::printf("#%zu %s %s -> %s (%zu octets)\n", ctx.packet->packet,
                            iso_time(ctx.packet->timestamp).c_str(), ctx.packet->source.c_str(),
                            ctx.packet->destination.c_str(), data.size());
                header_printed = true;
            }
        }
        if (!block.supported) {
            std::printf("%sCAT%03u data block, %zu octets (category not decoded)\n", indent, block.category,
                        block.length);
            continue;
        }
        for (const asterix::Record& record : block.records) {
            std::printf("%s%s\n", indent, asterix::summarize(record).c_str());
        }
    }

    for (const asterix::DecodeError& e : result.errors) {
        ++stats.errors;
        if (opt.json) {
            std::string line;
            asterix::json::Writer w(line, opt.indent);
            w.begin_object();
            context_members(w, ctx);
            w.key("error");
            asterix::json::write(w, e);
            w.end_object();
            line += '\n';
            std::fputs(line.c_str(), stdout);
        } else {
            std::fflush(stdout);
            std::fprintf(stderr, "%s: offset %zu: %s%s%s: %s\n", where(ctx).c_str(), e.offset,
                         asterix::to_string(e.code), e.item.empty() ? "" : " in ", e.item.c_str(),
                         e.message.c_str());
        }
    }
}

bool process_file(const std::string& name, const Options& opt, Statistics& stats) {
    std::vector<std::uint8_t> data;
    if (!read_file(name, data)) {
        std::fprintf(stderr, "asterix-dump: cannot read '%s': %s\n", name.c_str(), std::strerror(errno));
        return false;
    }
    Context ctx;
    ctx.source = name;

    InputFormat format = opt.format;
    if (format == InputFormat::Auto) {
        if (capture::detect_format(data.data(), data.size()) != capture::Format::Unknown) {
            format = InputFormat::Pcap;
        } else if (!data.empty() && looks_like_text(data)) {
            format = InputFormat::Hex;
        } else {
            format = InputFormat::Raw;
        }
    }

    switch (format) {
        case InputFormat::Pcap: {
            std::vector<capture::Datagram> datagrams;
            try {
                datagrams = capture::read_udp_datagrams(data.data(), data.size(), stats.capture);
                stats.had_capture = true;
            } catch (const capture::Error& e) {
                std::fprintf(stderr, "asterix-dump: %s: %s\n", name.c_str(), e.what());
                return false;
            }
            for (const capture::Datagram& d : datagrams) {
                if (opt.port && d.source_port != *opt.port && d.destination_port != *opt.port) {
                    continue;
                }
                ctx.packet = &d;
                process(d.payload, ctx, opt, stats);
            }
            return true;
        }
        case InputFormat::Hex: {
            std::string_view text(reinterpret_cast<const char*>(data.data()), data.size());
            std::size_t line_number = 0;
            bool ok = true;
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
                    std::fprintf(stderr, "asterix-dump: %s: line %zu: %s\n", name.c_str(), line_number,
                                 error.c_str());
                    ok = false;
                    continue;
                }
                if (bytes.empty()) {
                    continue;
                }
                ctx.line = line_number;
                process(bytes, ctx, opt, stats);
            }
            return ok;
        }
        case InputFormat::Raw:
        case InputFormat::Auto:
            process(data, ctx, opt, stats);
            return true;
    }
    return true;
}

void print_statistics(const Statistics& stats) {
    if (stats.had_capture) {
        std::fprintf(stderr, "packets: %zu, UDP datagrams: %zu, IP fragments skipped: %zu, other skipped: %zu\n",
                     stats.capture.packets, stats.capture.datagrams, stats.capture.fragments,
                     stats.capture.skipped);
    }
    std::fprintf(stderr, "datagrams decoded: %zu (%zu octets)\n", stats.datagrams, stats.octets);
    for (const auto& [category, blocks] : stats.blocks) {
        const auto it = stats.records.find(category);
        const std::size_t records = it == stats.records.end() ? 0 : it->second;
        if (asterix::is_supported_category(static_cast<std::uint8_t>(category))) {
            std::fprintf(stderr, "CAT%03u: %zu data blocks, %zu records\n", category, blocks, records);
        } else {
            std::fprintf(stderr, "CAT%03u: %zu data blocks (not decoded)\n", category, blocks);
        }
    }
    std::fprintf(stderr, "decode errors: %zu\n", stats.errors);
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse_options(argc, argv);
    Statistics stats;
    bool io_ok = true;

    for (const std::string& hex : opt.hex_arguments) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!parse_hex(hex, bytes, error)) {
            usage_error("invalid --hex value: " + error);
        }
        Context ctx;
        ctx.source = "--hex";
        process(bytes, ctx, opt, stats);
    }
    for (const std::string& file : opt.files) {
        io_ok = process_file(file, opt, stats) && io_ok;
    }

    std::fflush(stdout);
    if (opt.stats) {
        print_statistics(stats);
    }
    if (!io_ok) {
        return 2;
    }
    return stats.errors == 0 ? 0 : 1;
}
