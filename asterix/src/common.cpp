#include "asterix/types.hpp"
#include "detail.hpp"

#include <cstdio>

namespace asterix {

const char* version() noexcept {
    return "1.0.0";
}

std::string TimeOfDay::to_string() const {
    // Round to the nearest millisecond with integer arithmetic.
    const std::uint64_t total_ms = (std::uint64_t{raw} * 1000 + 64) / 128;
    const auto hours = static_cast<unsigned>(total_ms / 3600000);
    const auto minutes = static_cast<unsigned>(total_ms / 60000 % 60);
    const auto seconds = static_cast<unsigned>(total_ms / 1000 % 60);
    const auto millis = static_cast<unsigned>(total_ms % 1000);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02u:%02u:%02u.%03u", hours, minutes, seconds, millis);
    return buf;
}

std::string octal_code(std::uint16_t code) {
    std::string out(4, '0');
    for (int i = 3; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = static_cast<char>('0' + (code & 7u));
        code = static_cast<std::uint16_t>(code >> 3);
    }
    return out;
}

std::optional<std::int32_t> decode_mode_c_altitude(std::uint16_t reply) noexcept {
    // Bit order of the 12-bit reply, MSB first: C1 A1 C2 A2 C4 A4 B1 D1 B2 D2 B4 D4.
    const auto pulse = [reply](unsigned n) { return detail::bit(reply, n); };
    const bool c1 = pulse(12), a1 = pulse(11), c2 = pulse(10), a2 = pulse(9);
    const bool c4 = pulse(8), a4 = pulse(7), b1 = pulse(6), d1 = pulse(5);
    const bool b2 = pulse(4), d2 = pulse(3), b4 = pulse(2), d4 = pulse(1);

    if (d1) {
        // Mode S altitude with Q = 1: the other 11 bits, in transmission order,
        // are a binary count of 25 ft steps starting at -1000 ft.
        const std::int32_t n = static_cast<std::int32_t>((detail::bits(reply, 12, 6) << 4) | detail::bits(reply, 4, 1));
        return n * 25 - 1000;
    }

    // Gillham code: D2 D4 A1 A2 A4 B1 B2 B4 are a reflected Gray code of the
    // 500 ft band; C1 C2 C4 select one of five 100 ft steps within it, in
    // reverse order in odd bands.
    unsigned hundreds = 0;
    if (c1) hundreds ^= 7u;
    if (c2) hundreds ^= 3u;
    if (c4) hundreds ^= 1u;
    if ((hundreds & 5u) == 5u) hundreds ^= 2u;  // map 7 -> 5 and 5 -> 7
    if (hundreds < 1 || hundreds > 5) {
        return std::nullopt;
    }

    unsigned five_hundreds = 0;
    if (d2) five_hundreds ^= 0xFFu;
    if (d4) five_hundreds ^= 0x7Fu;
    if (a1) five_hundreds ^= 0x3Fu;
    if (a2) five_hundreds ^= 0x1Fu;
    if (a4) five_hundreds ^= 0x0Fu;
    if (b1) five_hundreds ^= 0x07u;
    if (b2) five_hundreds ^= 0x03u;
    if (b4) five_hundreds ^= 0x01u;
    if ((five_hundreds & 1u) != 0) {
        hundreds = 6 - hundreds;
    }
    return (static_cast<std::int32_t>(five_hundreds * 5 + hundreds) - 13) * 100;
}

const char* to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::TruncatedBlockHeader: return "TruncatedBlockHeader";
        case ErrorCode::InvalidBlockLength: return "InvalidBlockLength";
        case ErrorCode::TruncatedBlock: return "TruncatedBlock";
        case ErrorCode::TruncatedRecord: return "TruncatedRecord";
        case ErrorCode::EmptyRecord: return "EmptyRecord";
        case ErrorCode::UndefinedItem: return "UndefinedItem";
        case ErrorCode::UndefinedSubfield: return "UndefinedSubfield";
        case ErrorCode::InvalidExplicitLength: return "InvalidExplicitLength";
    }
    return "Unknown";
}

namespace detail {

std::uint32_t read_compound_presence(Reader& in) noexcept {
    std::uint32_t mask = 0;
    unsigned index = 0;
    for (;;) {
        const std::uint8_t octet = in.u8();
        for (unsigned b = 0; b < 7; ++b, ++index) {
            if (index < 32 && (octet & (0x80u >> b)) != 0) {
                mask |= std::uint32_t{1} << index;
            }
        }
        if ((octet & 1u) == 0 || in.remaining() == 0) {
            return mask;
        }
    }
}

std::string frn_label(std::size_t frn) {
    return "FRN " + std::to_string(frn);
}

namespace {

// Length of an FX-terminated octet chain, or 0 if it does not end within `avail`.
std::size_t fx_chain_length(const std::uint8_t* p, std::size_t avail) noexcept {
    for (std::size_t i = 0; i < avail; ++i) {
        if ((p[i] & 1u) == 0) {
            return i + 1;
        }
    }
    return 0;
}

std::size_t truncated(ScanError& error, std::size_t at, const char* what) {
    error.code = ErrorCode::TruncatedRecord;
    error.at = at;
    error.message = std::string(what) + " runs past the end of the data block";
    return 0;
}

}  // namespace

std::size_t item_size(Format format, std::uint8_t size, const Subfield* subfields,
                      std::size_t subfield_count, const std::uint8_t* p, std::size_t avail,
                      ScanError& error) {
    switch (format) {
        case Format::Fixed:
            if (avail < size) {
                return truncated(error, avail, "fixed-length item");
            }
            return size;

        case Format::Extended: {
            const std::size_t n = fx_chain_length(p, avail);
            if (n == 0) {
                return truncated(error, avail, "field extension chain");
            }
            return n;
        }

        case Format::Repetitive: {
            if (avail < 1) {
                return truncated(error, 0, "repetition factor");
            }
            const std::size_t n = 1 + std::size_t{p[0]} * size;
            if (avail < n) {
                return truncated(error, avail, "repetitive item");
            }
            return n;
        }

        case Format::Explicit: {
            if (avail < 1) {
                return truncated(error, 0, "length octet");
            }
            const std::size_t n = p[0];
            if (n == 0) {
                error.code = ErrorCode::InvalidExplicitLength;
                error.at = 0;
                error.message = "explicit length octet is zero";
                return 0;
            }
            if (avail < n) {
                return truncated(error, avail, "explicit-length item");
            }
            return n;
        }

        case Format::Compound: {
            const std::size_t primary = fx_chain_length(p, avail);
            if (primary == 0) {
                return truncated(error, avail, "compound item presence octets");
            }
            std::size_t pos = primary;
            for (std::size_t octet = 0; octet < primary; ++octet) {
                for (unsigned b = 0; b < 7; ++b) {
                    if ((p[octet] & (0x80u >> b)) == 0) {
                        continue;
                    }
                    const std::size_t index = octet * 7 + b;
                    if (index >= subfield_count || !subfields[index].defined) {
                        error.code = ErrorCode::UndefinedSubfield;
                        error.at = octet;
                        error.message = "compound item flags subfield #" + std::to_string(index + 1) +
                                        ", which the specification does not define";
                        return 0;
                    }
                    const Subfield& sub = subfields[index];
                    std::size_t n = sub.size;
                    if (sub.format == Format::Repetitive) {
                        if (avail - pos < 1) {
                            return truncated(error, avail, "subfield repetition factor");
                        }
                        n = 1 + std::size_t{p[pos]} * sub.size;
                    }
                    if (avail - pos < n) {
                        return truncated(error, avail, "compound subfield");
                    }
                    pos += n;
                }
            }
            return pos;
        }
    }
    return truncated(error, 0, "item");
}

}  // namespace detail
}  // namespace asterix
