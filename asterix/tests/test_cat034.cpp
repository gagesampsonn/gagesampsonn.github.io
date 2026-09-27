#include "helpers.hpp"

using namespace test;
using namespace asterix;
using namespace asterix::cat034;

TEST(cat034_sector_crossing_sample) {
    const auto r = single_record<cat034::Record>(cat034_example());
    CHECK(r.data_source == (DataSourceId{25, 14}));
    CHECK_EQ(r.message_type, MessageType::SectorCrossing);
    CHECK_EQ(r.time_of_day->seconds(), 29906.3359375);
    CHECK_EQ(r.sector_number->azimuth_deg(), 90.0);
    CHECK(!r.antenna_rotation_period);

    REQUIRE(r.system_configuration);
    const auto& s = *r.system_configuration;
    REQUIRE(s.com);
    CHECK(!s.com->nogo);
    CHECK(s.com->rdpc);
    CHECK(!s.com->rdpr);
    CHECK(!s.com->ovl_rdp);
    CHECK(!s.com->ovl_xmt);
    CHECK(!s.com->msc);
    CHECK(!s.com->tsv);
    CHECK(!s.psr);
    REQUIRE(s.ssr);
    CHECK(!s.ssr->ant);
    CHECK_EQ(s.ssr->chab, 2);
    CHECK(!s.ssr->ovl);
    CHECK(!s.mds);

    REQUIRE(r.processing_mode);
    REQUIRE(r.processing_mode->com);
    CHECK_EQ(r.processing_mode->com->red_rdp, 0);
    CHECK_EQ(r.processing_mode->com->red_xmt, 0);
    CHECK(!r.processing_mode->psr);
}

TEST(cat034_every_item) {
    const auto r = single_record<cat034::Record>(block(34, {record({
        {1, hex("01 02")},                          // I034/010
        {2, hex("01")},                             // I034/000 north marker
        {3, hex("00 00 80")},                       // I034/030 1 s
        {4, hex("FF")},                             // I034/020
        {5, hex("02 40")},                          // I034/041 4.5 s
        {6, hex("9C AA E8 30 CD 80")},              // I034/050 COM PSR SSR MDS
        {7, hex("9C 56 AC E0 90")},                 // I034/060 COM PSR SSR MDS
        {8, hex("02 0F FF A0 05")},                 // I034/070
        {9, hex("01 00 FF FF 20 00 E0 00")},        // I034/100
        {10, hex("05")},                            // I034/110
        {11, hex("FF F6 20 00 00 C0 00 00")},       // I034/120
        {12, hex("80 7F")},                         // I034/090
        {13, hex("03 AA BB")},                      // RE
        {14, hex("02 CC")},                         // SP
    })}));

    CHECK(r.data_source == (DataSourceId{1, 2}));
    CHECK_EQ(r.message_type, MessageType::NorthMarker);
    CHECK_EQ(r.time_of_day->to_string(), "00:00:01.000");
    CHECK_EQ(r.sector_number->azimuth_deg(), 358.59375);
    CHECK_EQ(r.antenna_rotation_period->seconds(), 4.5);

    const auto& s = *r.system_configuration;
    REQUIRE(s.com && s.psr && s.ssr && s.mds);
    CHECK(s.com->nogo);
    CHECK(!s.com->rdpc);
    CHECK(s.com->rdpr);
    CHECK(!s.com->ovl_rdp);
    CHECK(s.com->ovl_xmt);
    CHECK(!s.com->msc);
    CHECK(s.com->tsv);
    CHECK(s.psr->ant);
    CHECK_EQ(s.psr->chab, 3);
    CHECK(!s.psr->ovl);
    CHECK(s.psr->msc);
    CHECK(!s.ssr->ant);
    CHECK_EQ(s.ssr->chab, 1);
    CHECK(s.ssr->ovl);
    CHECK(!s.ssr->msc);
    CHECK(s.mds->ant);
    CHECK_EQ(s.mds->chab, 2);
    CHECK(!s.mds->ovl_sur);
    CHECK(s.mds->msc);
    CHECK(s.mds->scf);
    CHECK(!s.mds->dlf);
    CHECK(s.mds->ovl_scf);
    CHECK(s.mds->ovl_dlf);

    const auto& m = *r.processing_mode;
    REQUIRE(m.com && m.psr && m.ssr && m.mds);
    CHECK_EQ(m.com->red_rdp, 5);
    CHECK_EQ(m.com->red_xmt, 3);
    CHECK(m.psr->pol);
    CHECK_EQ(m.psr->red_rad, 2);
    CHECK_EQ(m.psr->stc, 3);
    CHECK_EQ(m.ssr->red_rad, 7);
    CHECK_EQ(m.mds->red_rad, 4);
    CHECK(m.mds->clu);

    REQUIRE(r.message_count_values && r.message_count_values->size() == 2);
    CHECK_EQ((*r.message_count_values)[0].typ, 1);
    CHECK_EQ((*r.message_count_values)[0].count, 2047);
    CHECK_EQ((*r.message_count_values)[1].typ, 20);
    CHECK_EQ((*r.message_count_values)[1].count, 5);
    CHECK_EQ(std::string(describe_counter(20)), "Mode S FRUIT (per sector)");

    CHECK_EQ(r.polar_window->rho_start_nm(), 1.0);
    CHECK_EQ(r.polar_window->rho_end_nm(), 255.99609375);
    CHECK_EQ(r.polar_window->theta_start_deg(), 45.0);
    CHECK_EQ(r.polar_window->theta_end_deg(), 315.0);

    CHECK_EQ(r.data_filter, DataFilter::SsrModeSPsr);

    CHECK_EQ(r.position->height, -10);
    CHECK_EQ(r.position->latitude_deg(), 45.0);
    CHECK_EQ(r.position->longitude_deg(), -90.0);

    CHECK_EQ(r.collimation_error->range_nm(), -1.0);
    CHECK_EQ(r.collimation_error->azimuth_deg(), 2.79052734375);

    CHECK(r.reserved_expansion == (Bytes{0xAA, 0xBB}));
    CHECK(r.special_purpose == (Bytes{0xCC}));
}

TEST(cat034_spare_subfield_is_an_error) {
    // I034/050 flags subfield #2, which the specification leaves spare.
    const auto result = decode(block(34, {record({{1, hex("01 02")}, {6, hex("C0 00 00")}})}));
    REQUIRE(result.errors.size() == 1);
    CHECK_EQ(result.errors[0].code, ErrorCode::UndefinedSubfield);
    CHECK_EQ(result.errors[0].item, "I034/050");
    CHECK_EQ(result.errors[0].category, 34);
    CHECK_EQ(result.errors[0].offset, 6u);  // the presence octet
    REQUIRE(result.blocks.size() == 1);
    CHECK(result.blocks[0].records.empty());
}

TEST(cat034_enum_descriptions) {
    CHECK_EQ(std::string(to_string(MessageType::NorthMarker)), "North marker message");
    CHECK_EQ(std::string(to_string(MessageType::ModeSJammingStrobe)), "Mode S Jamming Strobe Message");
    CHECK_EQ(std::string(to_string(static_cast<MessageType>(99))), "Unknown");
    CHECK_EQ(std::string(to_string(DataFilter::Weather)), "Filter for Weather data");
    CHECK_EQ(std::string(describe_counter(21)), "Reserved");
}
