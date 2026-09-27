#pragma once

// Value types and helpers shared by all ASTERIX categories.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace asterix {

/// Library version string ("major.minor.patch").
const char* version() noexcept;

/// SAC/SIC pair identifying a data source.
/// Used by I034/010, I048/010, I063/010 and I063/050.
struct DataSourceId {
    std::uint8_t sac = 0;  ///< System Area Code
    std::uint8_t sic = 0;  ///< System Identification Code

    friend bool operator==(const DataSourceId& a, const DataSourceId& b) noexcept {
        return a.sac == b.sac && a.sic == b.sic;
    }
    friend bool operator!=(const DataSourceId& a, const DataSourceId& b) noexcept {
        return !(a == b);
    }
};

/// UTC time of day, reset to zero at midnight. 24 bits, LSB = 1/128 s.
/// Used by I034/030, I048/140 and I063/030.
struct TimeOfDay {
    std::uint32_t raw = 0;

    /// Seconds since midnight UTC.
    double seconds() const noexcept { return raw / 128.0; }
    /// Formats the time as "HH:MM:SS.sss".
    std::string to_string() const;
};

/// An "element populated" flag plus value, as used by the newer extensions of
/// I048/020 and I063/060: `val` only carries information when `ep` is set.
struct PopulatedValue {
    bool ep = false;       ///< true when the element is populated
    std::uint8_t val = 0;  ///< the element value as transmitted

    /// The value if the element is populated, otherwise std::nullopt.
    std::optional<std::uint8_t> value() const noexcept {
        if (ep) {
            return val;
        }
        return std::nullopt;
    }
};

/// Formats a 12-bit Mode-2 or Mode-3/A code (bits A4 A2 A1 B4 B2 B1 C4 C2 C1
/// D4 D2 D1) as four octal digits, e.g. "7700".
std::string octal_code(std::uint16_t code);

/// Decodes a Mode C altitude reply given in the I048/100 bit order
/// (C1 A1 C2 A2 C4 A4 B1 D1 B2 D2 B4 D4, most significant bit first).
///
/// With D1 = 0 the reply is decoded as Gillham (Gray) code in 100 ft steps.
/// With D1 = 1 it is decoded as a Mode S altitude with the Q bit set (D1 is
/// the Q bit for Mode S, see the notes of I048/100): 25 ft steps.
///
/// Returns the pressure altitude in feet, or std::nullopt for an illegal code.
std::optional<std::int32_t> decode_mode_c_altitude(std::uint16_t reply) noexcept;

/// Kinds of decoding problems. Any error ends decoding of the affected data
/// block (record boundaries cannot be recovered after a malformed item); the
/// following data blocks are still decoded when their framing is intact.
enum class ErrorCode : std::uint8_t {
    TruncatedBlockHeader,   ///< fewer than 3 octets left where a data block header was expected
    InvalidBlockLength,     ///< LEN field smaller than 3
    TruncatedBlock,         ///< LEN field reaches past the end of the input
    TruncatedRecord,        ///< FSPEC or a data item reaches past the end of its data block
    EmptyRecord,            ///< FSPEC announces no data items
    UndefinedItem,          ///< FSPEC flags a field reference number the UAP does not define
    UndefinedSubfield,      ///< compound item flags a subfield the specification does not define
    InvalidExplicitLength,  ///< explicit-length item (RE/SP) with a length octet of zero
};

/// Stable identifier of an error code, e.g. "TruncatedRecord".
const char* to_string(ErrorCode code) noexcept;

/// A decoding problem and where it was found.
struct DecodeError {
    ErrorCode code = ErrorCode::TruncatedBlockHeader;
    std::size_t offset = 0;     ///< byte offset in the decoded buffer where the problem was detected
    std::uint8_t category = 0;  ///< category of the enclosing data block (0 if unknown)
    std::string item;           ///< data item involved, e.g. "I048/130" or "FSPEC" (may be empty)
    std::string message;        ///< human-readable description
};

}  // namespace asterix
