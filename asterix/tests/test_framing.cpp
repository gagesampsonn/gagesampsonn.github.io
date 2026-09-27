#include "helpers.hpp"

using namespace test;
using namespace asterix;

namespace {

// Returns a copy: callers often pass a temporary result.
DecodeError single_error(const DecodeResult& result) {
    REQUIRE(result.errors.size() == 1);
    return result.errors[0];
}

}  // namespace

TEST(framing_empty_input) {
    const auto result = decode(Bytes{});
    CHECK(result.ok());
    CHECK(result.blocks.empty());
    CHECK_EQ(result.consumed, 0u);
    CHECK(decode(nullptr, 10).blocks.empty());
}

TEST(framing_truncated_header) {
    const auto result = decode(hex("30 00"));
    const DecodeError e = single_error(result);
    CHECK_EQ(e.code, ErrorCode::TruncatedBlockHeader);
    CHECK_EQ(e.offset, 0u);
    CHECK(result.blocks.empty());
}

TEST(framing_length_smaller_than_header) {
    const DecodeError e = single_error(decode(hex("30 00 02 FF")));
    CHECK_EQ(e.code, ErrorCode::InvalidBlockLength);
    CHECK_EQ(e.offset, 1u);
    CHECK_EQ(e.category, 48);
}

TEST(framing_length_beyond_input) {
    Bytes data = cat048_example();
    data.pop_back();
    const auto result = decode(data);
    const DecodeError e = single_error(result);
    CHECK_EQ(e.code, ErrorCode::TruncatedBlock);
    CHECK(result.blocks.empty());
    CHECK_EQ(result.consumed, 0u);
}

TEST(framing_header_only_block) {
    const auto result = decode(hex("30 00 03"));
    CHECK(result.ok());
    REQUIRE(result.blocks.size() == 1);
    CHECK(result.blocks[0].records.empty());
    CHECK(result.blocks[0].supported);
    CHECK_EQ(result.consumed, 3u);
}

TEST(framing_multiple_blocks_and_categories) {
    const Bytes unsupported = hex("01 00 06 AA BB CC");  // CAT001: framed but not decoded
    const Bytes data = concat({cat034_example(), unsupported, cat048_example(),
                               block(63, {record({{1, hex("00 01")}})})});
    const auto result = decode(data);
    CHECK(result.ok());
    CHECK_EQ(result.consumed, data.size());
    REQUIRE(result.blocks.size() == 4);
    CHECK_EQ(result.blocks[0].category, 34);
    CHECK_EQ(result.blocks[0].offset, 0u);
    CHECK_EQ(result.blocks[0].length, 16u);
    CHECK_EQ(result.blocks[1].category, 1);
    CHECK(!result.blocks[1].supported);
    CHECK(result.blocks[1].records.empty());
    CHECK_EQ(result.blocks[1].offset, 16u);
    CHECK_EQ(result.blocks[2].category, 48);
    CHECK_EQ(result.blocks[2].offset, 22u);
    CHECK_EQ(result.blocks[3].category, 63);
    REQUIRE(result.blocks[3].records.size() == 1);
    CHECK_EQ(category_of(result.blocks[0].records[0]), 34);
    CHECK_EQ(category_of(result.blocks[2].records[0]), 48);
    CHECK_EQ(category_of(result.blocks[3].records[0]), 63);
    CHECK(is_supported_category(34) && is_supported_category(48) && is_supported_category(63));
    CHECK(!is_supported_category(62));
}

TEST(framing_partial_trailing_block_reports_consumed) {
    // A stream reader can keep data[consumed...] and retry when more arrives.
    Bytes data = concat({cat048_example(), cat034_example()});
    data.resize(data.size() - 5);
    const auto result = decode(data);
    CHECK_EQ(result.blocks.size(), 1u);
    CHECK_EQ(result.consumed, cat048_example().size());
    CHECK_EQ(single_error(result).code, ErrorCode::TruncatedBlock);
}

TEST(framing_error_in_one_block_does_not_affect_the_next) {
    const Bytes bad = block(48, {record({{4, hex("01 02")}})});  // I048/040 needs 4 octets
    const auto result = decode(concat({bad, cat034_example()}));
    const DecodeError e = single_error(result);
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "I048/040");
    CHECK_EQ(e.offset, bad.size());  // detected at the end of the block
    REQUIRE(result.blocks.size() == 2);
    CHECK(result.blocks[0].records.empty());
    CHECK_EQ(result.blocks[1].records.size(), 1u);
    CHECK_EQ(result.consumed, bad.size() + cat034_example().size());
}

TEST(framing_records_before_an_error_are_kept) {
    const Bytes good = record({{1, hex("01 02")}});
    const Bytes empty_fspec = hex("00");
    const auto result = decode(block(48, {good, good, empty_fspec, good}));
    const DecodeError e = single_error(result);
    CHECK_EQ(e.code, ErrorCode::EmptyRecord);
    CHECK_EQ(e.offset, 9u);
    REQUIRE(result.blocks.size() == 1);
    CHECK_EQ(result.blocks[0].records.size(), 2u);
}

TEST(record_fspec_runs_past_block) {
    const DecodeError e = single_error(decode(hex("30 00 05 81 01")));
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "FSPEC");
}

TEST(record_frn_beyond_uap) {
    // Fifth FSPEC octet flags FRN 29; CAT048 defines 28 FRNs.
    const DecodeError e = single_error(decode(hex("30 00 08 01 01 01 01 80")));
    CHECK_EQ(e.code, ErrorCode::UndefinedItem);
    CHECK_EQ(e.offset, 7u);
}

TEST(record_trailing_empty_fspec_octets_are_tolerated) {
    // FX set on octets that flag nothing: wasteful but unambiguous.
    const auto result = decode(hex("30 00 08 81 01 00 01 02"));
    CHECK(result.ok());
    REQUIRE(result.blocks.size() == 1 && result.blocks[0].records.size() == 1);
    CHECK(std::get<cat048::Record>(result.blocks[0].records[0]).data_source == (DataSourceId{1, 2}));
}

TEST(record_extended_item_without_terminating_octet) {
    const DecodeError e = single_error(decode(block(48, {record({{3, hex("01 01 01")}})})));
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "I048/020");
}

TEST(record_repetitive_item_overrun) {
    // REP = 2 BDS registers but only one present.
    const DecodeError e = single_error(decode(block(48, {record({{10, hex("02 00 00 00 00 00 00 00 40")}})})));
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "I048/250");
}

TEST(record_compound_subfield_overrun) {
    // I048/120 RDS announces 3 repetitions of 6 octets, block ends after one.
    const DecodeError e = single_error(decode(block(48, {record({{20, hex("40 03 00 01 00 02 00 03")}})})));
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "I048/120");
}

TEST(record_compound_undefined_subfield) {
    // I048/130 presence octets flag subfield #8, which does not exist.
    const DecodeError e = single_error(decode(block(48, {record({{7, hex("01 80 00")}})})));
    CHECK_EQ(e.code, ErrorCode::UndefinedSubfield);
    CHECK_EQ(e.item, "I048/130");
    CHECK_EQ(e.offset, 5u);
}

TEST(record_explicit_length_zero) {
    const DecodeError e = single_error(decode(block(48, {record({{27, hex("00")}})})));
    CHECK_EQ(e.code, ErrorCode::InvalidExplicitLength);
    CHECK_EQ(e.item, "I048/SP");
}

TEST(record_explicit_length_overrun) {
    const DecodeError e = single_error(decode(block(34, {record({{13, hex("05 01 02")}})})));
    CHECK_EQ(e.code, ErrorCode::TruncatedRecord);
    CHECK_EQ(e.item, "I034/RE");
}

TEST(error_code_names) {
    CHECK_EQ(std::string(to_string(ErrorCode::TruncatedBlockHeader)), "TruncatedBlockHeader");
    CHECK_EQ(std::string(to_string(ErrorCode::InvalidBlockLength)), "InvalidBlockLength");
    CHECK_EQ(std::string(to_string(ErrorCode::TruncatedBlock)), "TruncatedBlock");
    CHECK_EQ(std::string(to_string(ErrorCode::TruncatedRecord)), "TruncatedRecord");
    CHECK_EQ(std::string(to_string(ErrorCode::EmptyRecord)), "EmptyRecord");
    CHECK_EQ(std::string(to_string(ErrorCode::UndefinedItem)), "UndefinedItem");
    CHECK_EQ(std::string(to_string(ErrorCode::UndefinedSubfield)), "UndefinedSubfield");
    CHECK_EQ(std::string(to_string(ErrorCode::InvalidExplicitLength)), "InvalidExplicitLength");
}
