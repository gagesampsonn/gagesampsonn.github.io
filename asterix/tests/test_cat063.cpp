#include "helpers.hpp"

using namespace test;
using namespace asterix;
using namespace asterix::cat063;

TEST(cat063_every_item) {
    const Bytes data = block(63, {record({
        {1, hex("05 06")},        // I063/010
        {2, hex("2A")},           // I063/015
        {3, hex("00 01 00")},     // I063/030 2 s
        {4, hex("07 08")},        // I063/050
        {5, hex("6B AD E0")},     // I063/060 with both extensions
        {6, hex("FF 38")},        // I063/070 -200 ms
        {7, hex("00 64 FF 80")},  // I063/080
        {8, hex("80 00")},        // I063/081
        {9, hex("FF FF 00 40")},  // I063/090
        {10, hex("01 00")},       // I063/091
        {11, hex("FF 00")},       // I063/092
        {13, hex("02 99")},       // RE
        {14, hex("01")},          // SP, empty payload
    })});
    CHECK_EQ(data[3], 0xFF);  // FSPEC FF F6: FRN 12 (spare) not set
    CHECK_EQ(data[4], 0xF6);
    const auto r = single_record<cat063::Record>(data);

    CHECK(r.data_source == (DataSourceId{5, 6}));
    CHECK_EQ(r.service_identification, 42);
    CHECK_EQ(r.time_of_message->seconds(), 2.0);
    CHECK(r.sensor == (DataSourceId{7, 8}));

    const auto& s = *r.sensor_status;
    CHECK_EQ(s.con, ConnectionStatus::Degraded);
    CHECK(s.psr);
    CHECK(!s.ssr);
    CHECK(s.mds);
    CHECK(!s.ads);
    CHECK(s.mlt);
    REQUIRE(s.ext1 && s.ext2);
    CHECK(s.ext1->ops);
    CHECK(!s.ext1->odp);
    CHECK(s.ext1->oxt);
    CHECK(!s.ext1->msc);
    CHECK(s.ext1->tsv);
    CHECK(s.ext1->npw);
    CHECK(s.ext2->ttf.ep);
    CHECK_EQ(s.ext2->ttf.value(), 1);
    CHECK(s.ext2->spo.ep);
    CHECK_EQ(s.ext2->spo.value(), 0);

    CHECK_EQ(r.time_stamping_bias_ms, -200);
    CHECK_EQ(r.ssr_range_gain_bias->gain, 100);
    CHECK_NEAR(r.ssr_range_gain_bias->range_gain(), 0.001, 1e-15);
    CHECK_EQ(r.ssr_range_gain_bias->bias_nm(), -1.0);
    CHECK_EQ(r.ssr_azimuth_bias->degrees(), -180.0);
    CHECK_EQ(r.psr_range_gain_bias->gain, -1);
    CHECK_NEAR(r.psr_range_gain_bias->range_gain(), -1e-5, 1e-18);
    CHECK_EQ(r.psr_range_gain_bias->bias_nm(), 0.5);
    CHECK_EQ(r.psr_azimuth_bias->degrees(), 1.40625);
    CHECK_EQ(r.psr_elevation_bias->degrees(), -1.40625);
    CHECK(r.reserved_expansion == (Bytes{0x99}));
    REQUIRE(r.special_purpose);
    CHECK(r.special_purpose->empty());
}

TEST(cat063_minimal_status) {
    // Operational sensor, all GO, single octet I063/060.
    const auto r = single_record<cat063::Record>(block(63, {record({{1, hex("00 01")}, {4, hex("00 02")}, {5, hex("00")}})}));
    CHECK_EQ(r.sensor_status->con, ConnectionStatus::Operational);
    CHECK(!r.sensor_status->ext1);
    CHECK(!r.time_of_message);
}

TEST(cat063_spare_frn_is_an_error) {
    // FRN 12 is spare in the CAT063 UAP; its length is unknown, so decoding
    // of the block has to stop.
    const Bytes good = record({{1, hex("00 01")}});
    const Bytes bad = hex("01 08 00 01");  // FSPEC: FX, then FRN 12
    const auto result = decode(block(63, {good, bad}));
    REQUIRE(result.errors.size() == 1);
    CHECK_EQ(result.errors[0].code, ErrorCode::UndefinedItem);
    CHECK_EQ(result.errors[0].item, "FSPEC");
    CHECK_EQ(result.errors[0].offset, 7u);  // second FSPEC octet of the second record
    REQUIRE(result.blocks.size() == 1);
    CHECK_EQ(result.blocks[0].records.size(), 1u);
}

TEST(cat063_enum_descriptions) {
    CHECK_EQ(std::string(to_string(ConnectionStatus::NotConnected)), "Not currently connected");
}
