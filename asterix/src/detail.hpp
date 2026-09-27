#pragma once

// Internal machinery shared by the category decoders. Not part of the public API.
//
// Every category is described by a UAP table of ItemDef entries (one per FRN).
// The generic record decoder walks the FSPEC, asks item_size() for the exact
// extent of each present item (bounds checked against the data block), and
// hands the item's octets to its decode function. Decode functions therefore
// only ever see complete items; Reader additionally returns zeros past the end
// so that even a bug in a decode function cannot read outside the item.

#include "asterix/decoder.hpp"
#include "asterix/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace asterix::detail {

/// Non-owning view of an item's octets.
struct ByteView {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

/// Sequential big-endian reader over a ByteView.
class Reader {
public:
    explicit Reader(ByteView view) noexcept : data_(view.data), size_(view.size) {}

    std::size_t remaining() const noexcept { return pos_ < size_ ? size_ - pos_ : 0; }

    std::uint8_t u8() noexcept {
        const std::uint8_t v = pos_ < size_ ? data_[pos_] : std::uint8_t{0};
        ++pos_;
        return v;
    }

    std::uint16_t u16() noexcept { return static_cast<std::uint16_t>(be(2)); }
    std::uint32_t u24() noexcept { return static_cast<std::uint32_t>(be(3)); }
    std::uint32_t u32() noexcept { return static_cast<std::uint32_t>(be(4)); }

    /// Reads an n-octet big-endian unsigned value (n <= 8).
    std::uint64_t be(unsigned n) noexcept {
        std::uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i) {
            v = (v << 8) | u8();
        }
        return v;
    }

    /// The octets not read yet.
    std::vector<std::uint8_t> rest() {
        std::vector<std::uint8_t> out;
        if (pos_ < size_) {
            out.assign(data_ + pos_, data_ + size_);
            pos_ = size_;
        }
        return out;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
};

/// Bit `n` of `v`, numbered as in the ASTERIX specifications (bit 1 = LSB).
constexpr bool bit(std::uint64_t v, unsigned n) noexcept {
    return ((v >> (n - 1)) & 1u) != 0;
}

/// Bits `hi` down to `lo` of `v` (1-based, inclusive).
constexpr std::uint32_t bits(std::uint64_t v, unsigned hi, unsigned lo) noexcept {
    return static_cast<std::uint32_t>((v >> (lo - 1)) & ((std::uint64_t{1} << (hi - lo + 1)) - 1));
}

/// Narrowing helpers for fields known to fit.
constexpr std::uint8_t u8_bits(std::uint64_t v, unsigned hi, unsigned lo) noexcept {
    return static_cast<std::uint8_t>(bits(v, hi, lo));
}
constexpr std::uint16_t u16_bits(std::uint64_t v, unsigned hi, unsigned lo) noexcept {
    return static_cast<std::uint16_t>(bits(v, hi, lo));
}

/// Interprets the low `width` bits of `v` as a two's complement number (width <= 31).
constexpr std::int32_t sign_extend(std::uint32_t v, unsigned width) noexcept {
    const std::uint32_t sign = std::uint32_t{1} << (width - 1);
    const std::uint32_t x = v & ((sign << 1) - 1);
    return static_cast<std::int32_t>(x ^ sign) - static_cast<std::int32_t>(sign);
}

constexpr std::int8_t s8(std::uint32_t v) noexcept {
    return static_cast<std::int8_t>(sign_extend(v, 8));
}
constexpr std::int16_t s16(std::uint32_t v, unsigned width = 16) noexcept {
    return static_cast<std::int16_t>(sign_extend(v, width));
}

/// Reads the FX-extended presence octets of a compound item. Bit k of the
/// result is set when subfield k+1 is present (first 32 subfields).
std::uint32_t read_compound_presence(Reader& in) noexcept;

/// Structure of a data item, as far as needed to find its extent.
enum class Format : std::uint8_t {
    Fixed,       ///< `size` octets
    Extended,    ///< octets chained by their FX bit (also "repetitive fx" items)
    Repetitive,  ///< REP octet followed by REP * `size` octets
    Explicit,    ///< length octet (counting itself) followed by data
    Compound,    ///< FX-extended presence octets followed by the present subfields
};

/// A compound item subfield. Undefined (spare) subfield slots have defined == false.
struct Subfield {
    bool defined = false;
    Format format = Format::Fixed;  ///< Fixed or Repetitive
    std::uint8_t size = 0;
};

constexpr Subfield fixed_subfield(std::uint8_t size) noexcept { return {true, Format::Fixed, size}; }
constexpr Subfield repetitive_subfield(std::uint8_t size) noexcept { return {true, Format::Repetitive, size}; }
constexpr Subfield spare_subfield() noexcept { return {}; }

/// Why item_size() failed.
struct ScanError {
    ErrorCode code = ErrorCode::TruncatedRecord;
    std::size_t at = 0;  ///< offset relative to the start of the item
    std::string message;
};

/// Size in octets of the item at `p`, of which `avail` octets are readable.
/// Returns 0 and fills `error` when the item is malformed or incomplete.
std::size_t item_size(Format format, std::uint8_t size, const Subfield* subfields,
                      std::size_t subfield_count, const std::uint8_t* p, std::size_t avail,
                      ScanError& error);

/// One UAP entry (one FRN) of a category whose records decode into R.
template <class R>
struct ItemDef {
    const char* name;  ///< "I048/010"; nullptr for a spare FRN
    Format format;
    std::uint8_t size;
    const Subfield* subfields;
    std::size_t subfield_count;
    void (*decode)(ByteView, R&);

    static constexpr ItemDef fixed(const char* name, std::uint8_t size, void (*fn)(ByteView, R&)) {
        return {name, Format::Fixed, size, nullptr, 0, fn};
    }
    static constexpr ItemDef extended(const char* name, void (*fn)(ByteView, R&)) {
        return {name, Format::Extended, 0, nullptr, 0, fn};
    }
    static constexpr ItemDef repetitive(const char* name, std::uint8_t size, void (*fn)(ByteView, R&)) {
        return {name, Format::Repetitive, size, nullptr, 0, fn};
    }
    static constexpr ItemDef explicit_length(const char* name, void (*fn)(ByteView, R&)) {
        return {name, Format::Explicit, 0, nullptr, 0, fn};
    }
    template <std::size_t N>
    static constexpr ItemDef compound(const char* name, const Subfield (&subfields)[N],
                                      void (*fn)(ByteView, R&)) {
        return {name, Format::Compound, 0, subfields, N, fn};
    }
    static constexpr ItemDef spare() { return {nullptr, Format::Fixed, 0, nullptr, 0, nullptr}; }
};

std::string frn_label(std::size_t frn);

/// Decodes one record starting at input[pos]; the enclosing data block ends at
/// input[end]. On success advances `pos` past the record.
template <class R, std::size_t N>
bool decode_record(const std::uint8_t* input, std::size_t& pos, std::size_t end,
                   std::uint8_t category, const ItemDef<R> (&uap)[N], R& record,
                   DecodeError& error) {
    static_assert(N > 0 && N < 64, "UAP size must fit the presence mask");
    const auto fail = [&](ErrorCode code, std::size_t offset, std::string item, std::string message) {
        error.code = code;
        error.offset = offset;
        error.category = category;
        error.item = std::move(item);
        error.message = std::move(message);
        return false;
    };

    // FSPEC: FX-chained octets, 7 FRN flags each, most significant bit first.
    const std::size_t record_start = pos;
    std::uint64_t present = 0;
    for (std::size_t octet = 0;; ++octet) {
        if (pos >= end) {
            return fail(ErrorCode::TruncatedRecord, pos, "FSPEC",
                        "FSPEC runs past the end of the data block");
        }
        const std::uint8_t flags = input[pos++];
        for (unsigned b = 0; b < 7; ++b) {
            if ((flags & (0x80u >> b)) == 0) {
                continue;
            }
            const std::size_t frn = octet * 7 + b + 1;
            if (frn > N || uap[frn - 1].name == nullptr) {
                return fail(ErrorCode::UndefinedItem, pos - 1, "FSPEC",
                            "FSPEC flags " + frn_label(frn) + ", which the UAP does not define");
            }
            present |= std::uint64_t{1} << (frn - 1);
        }
        if ((flags & 1u) == 0) {
            break;
        }
    }
    if (present == 0) {
        return fail(ErrorCode::EmptyRecord, record_start, "FSPEC", "record contains no data items");
    }

    for (std::size_t i = 0; i < N; ++i) {
        if ((present & (std::uint64_t{1} << i)) == 0) {
            continue;
        }
        const ItemDef<R>& def = uap[i];
        ScanError scan;
        const std::size_t size = item_size(def.format, def.size, def.subfields, def.subfield_count,
                                           input + pos, end - pos, scan);
        if (size == 0) {
            return fail(scan.code, pos + scan.at, def.name, std::move(scan.message));
        }
        def.decode(ByteView{input + pos, size}, record);
        pos += size;
    }
    return true;
}

/// Decodes the records in input[begin, end) until the end of the block or the
/// first error.
template <class R, std::size_t N>
void decode_records(const std::uint8_t* input, std::size_t begin, std::size_t end,
                    std::uint8_t category, const ItemDef<R> (&uap)[N],
                    std::vector<Record>& records, std::vector<DecodeError>& errors) {
    std::size_t pos = begin;
    while (pos < end) {
        R record;
        DecodeError error;
        if (!decode_record(input, pos, end, category, uap, record, error)) {
            errors.push_back(std::move(error));
            return;
        }
        records.emplace_back(std::move(record));
    }
}

// Per-category entry points, defined in catNNN.cpp.
void decode_cat034(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors);
void decode_cat048(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors);
void decode_cat063(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors);

}  // namespace asterix::detail
