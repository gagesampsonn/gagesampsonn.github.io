#pragma once

// JSON serialization of decoded data.
//
// Records are written as objects keyed by data item ("I010", "I140", ...,
// "SP", "RE") in UAP order. Subfield names follow the EUROCONTROL mnemonics
// (SAC, SIC, TYP, RHO, THETA, ...). Quantities are written in the unit of the
// specification (NM, degrees, seconds, FL, ...), codes and flags as integers,
// Mode-2/Mode-3/A codes as octal strings, and addresses / opaque bit fields
// (I048/220, MBDATA, I048/260, RE, SP) as upper-case hexadecimal strings.

#include "asterix/decoder.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace asterix::json {

/// Minimal streaming JSON writer appending to a std::string.
class Writer {
public:
    /// `indent` < 0 produces compact single-line output; otherwise output is
    /// pretty-printed with `indent` spaces per nesting level.
    explicit Writer(std::string& out, int indent = -1);

    Writer& begin_object();
    Writer& end_object();
    Writer& begin_array();
    Writer& end_array();
    /// Starts an object member; must be followed by exactly one value.
    Writer& key(std::string_view name);

    Writer& value(std::string_view text);
    Writer& value(const char* text) { return value(std::string_view(text)); }
    Writer& value(const std::string& text) { return value(std::string_view(text)); }
    Writer& value(bool b);
    Writer& value(double number);
    Writer& null();

    template <class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Writer& value(T number) {
        if constexpr (std::is_signed_v<T>) {
            return signed_value(static_cast<long long>(number));
        } else {
            return unsigned_value(static_cast<unsigned long long>(number));
        }
    }

private:
    Writer& signed_value(long long number);
    Writer& unsigned_value(unsigned long long number);
    void before_value();
    void newline();

    struct Level {
        bool empty = true;
    };
    std::string& out_;
    int indent_;
    std::vector<Level> stack_;
    bool after_key_ = false;
};

void write(Writer& w, const cat034::Record& record);
void write(Writer& w, const cat048::Record& record);
void write(Writer& w, const cat063::Record& record);
void write(Writer& w, const Record& record);

/// Writes the members of a data block ("category", "offset", "length",
/// "supported" when false, "records") into the currently open object. Useful
/// for adding context members (packet number, timestamp, ...) alongside.
void write_members(Writer& w, const DataBlock& block);
void write(Writer& w, const DataBlock& block);
void write(Writer& w, const DecodeError& error);
/// {"blocks": [...], "errors": [...]}
void write(Writer& w, const DecodeResult& result);

}  // namespace asterix::json

namespace asterix {

/// JSON text; `indent` < 0 gives compact single-line output.
std::string to_json(const DecodeResult& result, int indent = -1);
std::string to_json(const DataBlock& block, int indent = -1);
std::string to_json(const Record& record, int indent = -1);

}  // namespace asterix
