#pragma once

// Helpers for building ASTERIX test data by hand.

#include "test.hpp"

#include <asterix/asterix.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace test {

using Bytes = std::vector<std::uint8_t>;

/// Parses hex digits, ignoring spaces.
inline Bytes hex(std::string_view text) {
    Bytes out;
    int high = -1;
    for (const char c : text) {
        int v = -1;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else continue;
        if (high < 0) {
            high = v;
        } else {
            out.push_back(static_cast<std::uint8_t>(high * 16 + v));
            high = -1;
        }
    }
    return out;
}

inline Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

/// Builds a record from {FRN, item octets} pairs: computes the FSPEC and
/// appends the items in FRN order.
inline Bytes record(const std::map<int, Bytes>& items) {
    const int max_frn = items.empty() ? 1 : items.rbegin()->first;
    const std::size_t octets = static_cast<std::size_t>((max_frn + 6) / 7);
    Bytes fspec(octets, 0);
    for (const auto& item : items) {
        const auto index = static_cast<std::size_t>((item.first - 1) / 7);
        fspec[index] = static_cast<std::uint8_t>(fspec[index] | (0x80u >> ((item.first - 1) % 7)));
    }
    for (std::size_t i = 0; i + 1 < octets; ++i) fspec[i] = static_cast<std::uint8_t>(fspec[i] | 1u);
    Bytes out = fspec;
    for (const auto& item : items) out.insert(out.end(), item.second.begin(), item.second.end());
    return out;
}

/// Wraps records into a data block with a correct LEN field.
inline Bytes block(std::uint8_t category, std::initializer_list<Bytes> records) {
    Bytes body;
    for (const Bytes& r : records) body.insert(body.end(), r.begin(), r.end());
    const std::size_t len = body.size() + 3;
    Bytes out{category, static_cast<std::uint8_t>(len >> 8), static_cast<std::uint8_t>(len & 0xFF)};
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

/// Decodes `data`, requires a clean single-record result and returns the record.
template <class R>
R single_record(const Bytes& data) {
    const asterix::DecodeResult result = asterix::decode(data);
    for (const auto& e : result.errors) {
        fail(__FILE__, __LINE__, "unexpected decode error at offset " + std::to_string(e.offset) + ": " +
                                     asterix::to_string(e.code) + " " + e.item + " " + e.message);
    }
    REQUIRE(result.errors.empty());
    REQUIRE(result.blocks.size() == 1);
    REQUIRE(result.blocks[0].records.size() == 1);
    const R* r = std::get_if<R>(&result.blocks[0].records[0]);
    REQUIRE(r != nullptr);
    return *r;
}

/// The widely published CAT048 example: one Mode S roll-call plot of DLH65A.
inline Bytes cat048_example() {
    return hex("30 00 30 fd f7 02 19 c9 35 6d 4d a0 c5 af f1 e0 02 00 05 28 3c 66 0c 10 c2 36 d4 18 20 01 c0 78"
               "00 31 bc 00 00 40 0d eb 07 b9 58 2e 41 00 20 f5");
}

/// A sector crossing message (real sample data).
inline Bytes cat034_example() {
    return hex("22 00 10 f6 19 0e 02 3a 69 2b 40 88 40 40 80 00");
}

}  // namespace test
