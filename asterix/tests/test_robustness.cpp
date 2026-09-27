// Randomised tests: structurally valid random records must decode cleanly,
// and arbitrary corruption must never crash, hang or read out of bounds
// (run under -DASTERIX_SANITIZE=ON to have ASan/UBSan check the latter).

#include "helpers.hpp"

using namespace test;
using namespace asterix;

namespace {

// Small deterministic PRNG (xorshift64*), identical on every platform.
class Random {
public:
    explicit Random(std::uint64_t seed) : state_(seed ? seed : 1) {}
    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 2685821657736338717ull;
    }
    unsigned below(unsigned n) { return static_cast<unsigned>(next() % n); }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next() >> 56); }
    bool chance(unsigned percent) { return below(100) < percent; }

private:
    std::uint64_t state_;
};

// Item structure as written in the specifications, independent of the
// library's own UAP tables.
struct Sub {
    char kind;  // 'F' fixed, 'R' repetitive, '-' spare
    unsigned size;
};
struct ItemSpec {
    char kind;  // 'F' fixed, 'E' extended, 'R' repetitive, 'X' explicit, 'C' compound, '-' spare
    unsigned size;  // fixed size, repetition size, or maximum number of extents
    std::vector<Sub> subs;
};

const std::vector<ItemSpec> kCat048 = {
    {'F', 2, {}}, {'F', 3, {}}, {'E', 6, {}}, {'F', 4, {}}, {'F', 2, {}}, {'F', 2, {}},
    {'C', 0, {{'F', 1}, {'F', 1}, {'F', 1}, {'F', 1}, {'F', 1}, {'F', 1}, {'F', 1}}},
    {'F', 3, {}}, {'F', 6, {}}, {'R', 8, {}}, {'F', 2, {}}, {'F', 4, {}}, {'F', 4, {}}, {'E', 2, {}},
    {'F', 4, {}}, {'E', 5, {}}, {'F', 2, {}}, {'F', 4, {}}, {'F', 2, {}}, {'C', 0, {{'F', 2}, {'R', 6}}},
    {'F', 2, {}}, {'F', 7, {}}, {'F', 1, {}}, {'F', 2, {}}, {'F', 1, {}}, {'F', 2, {}}, {'X', 0, {}},
    {'X', 0, {}},
};

const std::vector<ItemSpec> kCat034 = {
    {'F', 2, {}}, {'F', 1, {}}, {'F', 3, {}}, {'F', 1, {}}, {'F', 2, {}},
    {'C', 0, {{'F', 1}, {'-', 0}, {'-', 0}, {'F', 1}, {'F', 1}, {'F', 2}}},
    {'C', 0, {{'F', 1}, {'-', 0}, {'-', 0}, {'F', 1}, {'F', 1}, {'F', 1}}},
    {'R', 2, {}}, {'F', 8, {}}, {'F', 1, {}}, {'F', 8, {}}, {'F', 2, {}}, {'X', 0, {}}, {'X', 0, {}},
};

const std::vector<ItemSpec> kCat063 = {
    {'F', 2, {}}, {'F', 1, {}}, {'F', 3, {}}, {'F', 2, {}}, {'E', 3, {}}, {'F', 2, {}}, {'F', 4, {}},
    {'F', 2, {}}, {'F', 4, {}}, {'F', 2, {}}, {'F', 2, {}}, {'-', 0, {}}, {'X', 0, {}}, {'X', 0, {}},
};

void random_bytes(Random& rng, Bytes& out, unsigned n) {
    for (unsigned i = 0; i < n; ++i) out.push_back(rng.byte());
}

Bytes random_item(Random& rng, const ItemSpec& spec) {
    Bytes out;
    switch (spec.kind) {
        case 'F':
            random_bytes(rng, out, spec.size);
            break;
        case 'E': {
            const unsigned extents = 1 + rng.below(spec.size);
            for (unsigned i = 0; i < extents; ++i) {
                const auto fx = static_cast<std::uint8_t>(i + 1 < extents ? 1 : 0);
                out.push_back(static_cast<std::uint8_t>((rng.byte() & 0xFE) | fx));
            }
            break;
        }
        case 'R': {
            const unsigned rep = rng.below(4);
            out.push_back(static_cast<std::uint8_t>(rep));
            random_bytes(rng, out, rep * spec.size);
            break;
        }
        case 'X': {
            const unsigned len = 1 + rng.below(6);
            out.push_back(static_cast<std::uint8_t>(len));
            random_bytes(rng, out, len - 1);
            break;
        }
        case 'C': {
            std::uint8_t presence = 0;
            Bytes fields;
            for (std::size_t i = 0; i < spec.subs.size(); ++i) {
                const Sub& sub = spec.subs[i];
                if (sub.kind == '-' || !rng.chance(60)) continue;
                presence = static_cast<std::uint8_t>(presence | (0x80u >> i));
                if (sub.kind == 'F') {
                    random_bytes(rng, fields, sub.size);
                } else {
                    const unsigned rep = rng.below(3);
                    fields.push_back(static_cast<std::uint8_t>(rep));
                    random_bytes(rng, fields, rep * sub.size);
                }
            }
            out.push_back(presence);
            out.insert(out.end(), fields.begin(), fields.end());
            break;
        }
    }
    return out;
}

Bytes random_record(Random& rng, const std::vector<ItemSpec>& uap) {
    std::map<int, Bytes> items;
    for (std::size_t i = 0; i < uap.size(); ++i) {
        if (uap[i].kind != '-' && rng.chance(50)) {
            items[static_cast<int>(i + 1)] = random_item(rng, uap[i]);
        }
    }
    if (items.empty()) {
        items[1] = random_item(rng, uap[0]);
    }
    return record(items);
}

Bytes random_block(Random& rng, std::uint8_t category, const std::vector<ItemSpec>& uap, unsigned records) {
    Bytes body;
    for (unsigned i = 0; i < records; ++i) {
        const Bytes r = random_record(rng, uap);
        body.insert(body.end(), r.begin(), r.end());
    }
    const std::size_t len = body.size() + 3;
    Bytes out{category, static_cast<std::uint8_t>(len >> 8), static_cast<std::uint8_t>(len)};
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// Structural invariants that must hold for any input.
void check_invariants(const Bytes& data, const DecodeResult& result) {
    CHECK(result.consumed <= data.size());
    std::size_t expected_offset = 0;
    for (const DataBlock& b : result.blocks) {
        CHECK_EQ(b.offset, expected_offset);
        CHECK(b.length >= 3 && b.offset + b.length <= data.size());
        CHECK_EQ(b.category, data[b.offset]);
        expected_offset = b.offset + b.length;
    }
    CHECK_EQ(result.consumed, expected_offset);
    for (const DecodeError& e : result.errors) {
        CHECK(e.offset <= data.size());
        CHECK(!e.message.empty());
    }
}

// Exercises every output path on whatever was decoded.
std::size_t render(const DecodeResult& result) {
    std::size_t total = to_json(result).size() + to_json(result, 2).size();
    for (const DataBlock& b : result.blocks) {
        for (const Record& r : b.records) total += summarize(r).size();
    }
    return total;
}

}  // namespace

TEST(robustness_random_valid_records_decode_cleanly) {
    Random rng(0xA57E81C5u);
    std::size_t records = 0;
    for (int i = 0; i < 3000; ++i) {
        const unsigned n = 1 + rng.below(4);
        const Bytes data = concat({random_block(rng, 48, kCat048, n), random_block(rng, 34, kCat034, n),
                                   random_block(rng, 63, kCat063, n)});
        const DecodeResult result = decode(data);
        if (!result.ok()) {
            const auto& e = result.errors[0];
            fail(__FILE__, __LINE__, std::string("valid data rejected: ") + to_string(e.code) + " " + e.item +
                                         " " + e.message);
            return;
        }
        REQUIRE(result.blocks.size() == 3);
        for (const DataBlock& b : result.blocks) {
            REQUIRE(b.records.size() == n);
            records += b.records.size();
        }
        CHECK_EQ(result.consumed, data.size());
        check_invariants(data, result);
        CHECK(render(result) > 0);
    }
    CHECK(records >= 3000 * 3);
}

TEST(robustness_mutated_input_never_misbehaves) {
    Random rng(20260927u);
    std::vector<Bytes> seeds = {cat048_example(), cat034_example()};
    for (int i = 0; i < 20; ++i) {
        seeds.push_back(concat({random_block(rng, 48, kCat048, 2), random_block(rng, 34, kCat034, 1),
                                random_block(rng, 63, kCat063, 1)}));
    }

    std::size_t decoded_records = 0;
    for (int i = 0; i < 60000; ++i) {
        Bytes data = seeds[rng.below(static_cast<unsigned>(seeds.size()))];
        const unsigned mutations = 1 + rng.below(4);
        for (unsigned m = 0; m < mutations && !data.empty(); ++m) {
            const unsigned pos = rng.below(static_cast<unsigned>(data.size()));
            switch (rng.below(6)) {
                case 0: data[pos] = static_cast<std::uint8_t>(data[pos] ^ (1u << rng.below(8))); break;
                case 1: data[pos] = rng.byte(); break;
                case 2: data.resize(pos); break;
                case 3: data.insert(data.begin() + pos, rng.byte()); break;
                case 4: data.erase(data.begin() + pos); break;
                case 5:  // rewrite a LEN field-sized pair
                    if (pos + 1 < data.size()) {
                        data[pos] = rng.byte() & 0x01;
                        data[pos + 1] = rng.byte();
                    }
                    break;
            }
        }
        const DecodeResult result = decode(data);
        check_invariants(data, result);
        render(result);
        for (const DataBlock& b : result.blocks) decoded_records += b.records.size();
    }
    CHECK(decoded_records > 0);
}

TEST(robustness_random_garbage) {
    Random rng(7);
    for (int i = 0; i < 20000; ++i) {
        Bytes data;
        random_bytes(rng, data, rng.below(64));
        if (!data.empty() && rng.chance(70)) {
            // Plausible header so that decoding reaches the record level.
            const std::uint8_t categories[] = {34, 48, 63};
            data[0] = categories[rng.below(3)];
            if (data.size() >= 3) {
                data[1] = 0;
                data[2] = static_cast<std::uint8_t>(data.size());
            }
        }
        const DecodeResult result = decode(data);
        check_invariants(data, result);
        render(result);
    }
}
