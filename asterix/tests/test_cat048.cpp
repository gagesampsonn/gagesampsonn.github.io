#include "helpers.hpp"

using namespace test;
using namespace asterix;
using namespace asterix::cat048;

TEST(cat048_published_example) {
    const auto r = single_record<cat048::Record>(cat048_example());

    REQUIRE(r.data_source);
    CHECK_EQ(r.data_source->sac, 25);
    CHECK_EQ(r.data_source->sic, 201);

    REQUIRE(r.time_of_day);
    CHECK_EQ(r.time_of_day->raw, 0x356D4Du);
    CHECK_EQ(r.time_of_day->seconds(), 27354.6015625);
    CHECK_EQ(r.time_of_day->to_string(), "07:35:54.602");

    REQUIRE(r.target_report_descriptor);
    CHECK_EQ(r.target_report_descriptor->typ, DetectionType::SingleModeSRollCall);
    CHECK(!r.target_report_descriptor->sim);
    CHECK(!r.target_report_descriptor->rdp);
    CHECK(!r.target_report_descriptor->spi);
    CHECK(!r.target_report_descriptor->rab);
    CHECK(!r.target_report_descriptor->ext1);

    REQUIRE(r.measured_position);
    CHECK_EQ(r.measured_position->rho_nm(), 197.68359375);
    CHECK_EQ(r.measured_position->theta_deg(), 340.13671875);

    REQUIRE(r.mode3a);
    CHECK_EQ(r.mode3a->to_string(), "1000");
    CHECK(!r.mode3a->v);
    CHECK(!r.mode3a->g);
    CHECK(!r.mode3a->l);

    REQUIRE(r.flight_level);
    CHECK_EQ(r.flight_level->fl(), 330.0);
    CHECK_EQ(r.flight_level->feet(), 33000.0);

    CHECK(!r.radar_plot_characteristics);
    CHECK_EQ(r.aircraft_address, 0x3C660Cu);
    CHECK_EQ(r.aircraft_identification, std::string("DLH65A  "));

    REQUIRE(r.bds_register_data);
    REQUIRE(r.bds_register_data->size() == 1);
    CHECK_EQ((*r.bds_register_data)[0].mb_data, 0xC0780031BC0000ull);
    CHECK_EQ((*r.bds_register_data)[0].bds1, 4);
    CHECK_EQ((*r.bds_register_data)[0].bds2, 0);

    CHECK_EQ(r.track_number, 3563);
    CHECK(!r.calculated_position);

    REQUIRE(r.calculated_velocity);
    CHECK_EQ(r.calculated_velocity->groundspeed_nm_s(), 1977.0 / 16384.0);
    CHECK_NEAR(r.calculated_velocity->groundspeed_kt(), 434.40, 0.01);
    CHECK_EQ(r.calculated_velocity->heading_deg(), 124.002685546875);

    REQUIRE(r.track_status);
    CHECK(!r.track_status->cnf);
    CHECK_EQ(r.track_status->rad, TrackSensorType::SsrModeS);
    CHECK(!r.track_status->dou);
    CHECK(!r.track_status->mah);
    CHECK_EQ(r.track_status->cdm, ClimbDescentMode::Maintaining);
    REQUIRE(r.track_status->ext1);
    CHECK(!r.track_status->ext1->tre);
    CHECK(!r.track_status->ext1->gho);

    REQUIRE(r.comms_acas_capability);
    CHECK_EQ(r.comms_acas_capability->com, CommCapability::CommAB);
    CHECK_EQ(r.comms_acas_capability->stat, FlightStatus::NoAlertNoSpiAirborne);
    CHECK(!r.comms_acas_capability->si);
    CHECK(r.comms_acas_capability->mssc);
    CHECK(r.comms_acas_capability->arc);
    CHECK(r.comms_acas_capability->aic);
    CHECK(r.comms_acas_capability->b1a);
    CHECK_EQ(r.comms_acas_capability->b1b, 5);

    CHECK(!r.track_quality);
    CHECK(!r.warning_error_conditions);
    CHECK(!r.mode_c);
    CHECK(!r.special_purpose);
    CHECK(!r.reserved_expansion);
}

// Every data item of the UAP in one record, values chosen to exercise sign
// handling, spare bits and all extensions.
TEST(cat048_every_item) {
    const Bytes data = block(48, {record({
        {1, hex("08 11")},                                        // I048/010
        {2, hex("A8 BF FF")},                                     // I048/140 86399.9921875 s
        {3, hex("F5 AD E5 97 8D D0")},                            // I048/020 with 5 extensions
        {4, hex("80 00 40 00")},                                  // I048/040 128 NM, 90 deg
        {5, hex("AF C0")},                                        // I048/070 V L 7700
        {6, hex("7F CF")},                                        // I048/090 G, FL -12.25
        {7, hex("FE 10 05 C4 20 B5 F0 08")},                      // I048/130 all subfields
        {8, hex("AB CD EF")},                                     // I048/220
        {9, hex("2C C3 71 CB 3D 20")},                            // I048/240 "KLM1234 "
        {10, hex("02 01 23 45 67 89 AB CD 60 FF FF FF FF FF FF FF 40")},  // I048/250
        {11, hex("FF FF")},                                       // I048/161 spare bits set
        {12, hex("FF FF 7F FF")},                                 // I048/042
        {13, hex("40 00 80 00")},                                 // I048/200
        {14, hex("B5 B0")},                                       // I048/170
        {15, hex("80 40 10 20")},                                 // I048/210
        {16, hex("03 4B FE")},                                    // I048/030 codes 1, 37, 127
        {17, hex("F1 23")},                                       // I048/080
        {18, hex("C6 29 0A BC")},                                 // I048/100 FL330 in Gillham code
        {19, hex("3F 9C")},                                       // I048/110 -100 * 25 ft
        {20, hex("C0 83 FB 01 01 02 03 04 05 06")},               // I048/120 CAL and RDS
        {21, hex("96 5A")},                                       // I048/230
        {22, hex("11 22 33 44 55 66 77")},                        // I048/260
        {23, hex("56")},                                          // I048/055
        {24, hex("02 9C")},                                       // I048/050 1234
        {25, hex("15")},                                          // I048/065
        {26, hex("0F FF")},                                       // I048/060
        {27, hex("04 01 02 03")},                                 // SP
        {28, hex("01")},                                          // RE, empty payload
    })});
    CHECK_EQ(data[3], 0xFF);  // FSPEC: FF FF FF FE
    CHECK_EQ(data[6], 0xFE);
    const auto r = single_record<cat048::Record>(data);

    CHECK(r.data_source == (DataSourceId{8, 17}));
    CHECK_EQ(r.time_of_day->seconds(), 86399.9921875);
    CHECK_EQ(r.time_of_day->to_string(), "23:59:59.992");

    const auto& d = *r.target_report_descriptor;
    CHECK_EQ(d.typ, DetectionType::ModeSRollCallPsr);
    CHECK(d.sim);
    CHECK(!d.rdp);
    CHECK(d.spi);
    CHECK(!d.rab);
    REQUIRE(d.ext1 && d.ext2 && d.ext3 && d.ext4 && d.ext5);
    CHECK(d.ext1->tst);
    CHECK(!d.ext1->err);
    CHECK(d.ext1->xpp);
    CHECK(!d.ext1->me);
    CHECK(d.ext1->mi);
    CHECK_EQ(d.ext1->foe_fri, FoeFri::UnknownTarget);
    CHECK(d.ext2->adsb.ep);
    CHECK_EQ(d.ext2->adsb.val, 1);
    CHECK(d.ext2->scn.ep);
    CHECK_EQ(d.ext2->scn.val, 0);
    CHECK(!d.ext2->pai.ep);
    CHECK_EQ(d.ext2->pai.val, 1);
    CHECK(!d.ext2->pai.value());
    CHECK(d.ext3->acasxv.ep);
    CHECK_EQ(d.ext3->acasxv.val, 2);
    CHECK(d.ext3->poxpr.ep);
    CHECK_EQ(d.ext3->poxpr.val, 1);
    CHECK(d.ext4->poact.ep);
    CHECK_EQ(d.ext4->poact.val, 0);
    CHECK(!d.ext4->dtfxpr.ep);
    CHECK(d.ext4->dtfact.ep);
    CHECK_EQ(d.ext4->dtfact.val, 1);
    CHECK(d.ext5->irmxpr.ep);
    CHECK_EQ(d.ext5->irmxpr.val, 1);
    CHECK(!d.ext5->irmact.ep);
    CHECK_EQ(d.ext5->irmact.val, 1);

    CHECK_EQ(r.measured_position->rho_nm(), 128.0);
    CHECK_EQ(r.measured_position->theta_deg(), 90.0);

    CHECK(r.mode3a->v);
    CHECK(!r.mode3a->g);
    CHECK(r.mode3a->l);
    CHECK_EQ(r.mode3a->code, 07700);
    CHECK_EQ(r.mode3a->to_string(), "7700");

    CHECK(!r.flight_level->v);
    CHECK(r.flight_level->g);
    CHECK_EQ(r.flight_level->level, -49);
    CHECK_EQ(r.flight_level->fl(), -12.25);

    const auto& c = *r.radar_plot_characteristics;
    CHECK_EQ(c.srl, 16);
    CHECK_EQ(c.srl_deg(), 0.703125);
    CHECK_EQ(c.srr, 5);
    CHECK_EQ(c.sam, -60);
    CHECK_EQ(c.prl_deg(), 1.40625);
    CHECK_EQ(c.pam, -75);
    CHECK_EQ(c.rpd, -16);
    CHECK_EQ(c.rpd_nm(), -0.0625);
    CHECK_EQ(c.apd, 8);
    CHECK_EQ(c.apd_deg(), 0.17578125);

    CHECK_EQ(r.aircraft_address, 0xABCDEFu);
    CHECK_EQ(r.aircraft_identification, std::string("KLM1234 "));

    REQUIRE(r.bds_register_data && r.bds_register_data->size() == 2);
    CHECK_EQ((*r.bds_register_data)[0].mb_data, 0x0123456789ABCDull);
    CHECK_EQ((*r.bds_register_data)[0].bds1, 6);
    CHECK_EQ((*r.bds_register_data)[0].bds2, 0);
    CHECK_EQ((*r.bds_register_data)[1].mb_data, 0xFFFFFFFFFFFFFFull);
    CHECK_EQ((*r.bds_register_data)[1].bds1, 4);

    CHECK_EQ(r.track_number, 4095);

    CHECK_EQ(r.calculated_position->x, -1);
    CHECK_EQ(r.calculated_position->x_nm(), -0.0078125);
    CHECK_EQ(r.calculated_position->y_nm(), 255.9921875);

    CHECK_EQ(r.calculated_velocity->groundspeed_nm_s(), 1.0);
    CHECK_EQ(r.calculated_velocity->groundspeed_kt(), 3600.0);
    CHECK_EQ(r.calculated_velocity->heading_deg(), 180.0);

    const auto& s = *r.track_status;
    CHECK(s.cnf);
    CHECK_EQ(s.rad, TrackSensorType::Psr);
    CHECK(s.dou);
    CHECK(!s.mah);
    CHECK_EQ(s.cdm, ClimbDescentMode::Descending);
    REQUIRE(s.ext1);
    CHECK(s.ext1->tre);
    CHECK(!s.ext1->gho);
    CHECK(s.ext1->sup);
    CHECK(s.ext1->tcc);

    CHECK_EQ(r.track_quality->sigma_x_nm(), 1.0);
    CHECK_EQ(r.track_quality->sigma_y_nm(), 0.5);
    CHECK_EQ(r.track_quality->sigma_v_nm_s(), 16.0 / 16384.0);
    CHECK_EQ(r.track_quality->sigma_h_deg(), 2.8125);

    REQUIRE(r.warning_error_conditions);
    CHECK(*r.warning_error_conditions == (std::vector<std::uint8_t>{1, 37, 127}));
    CHECK_EQ(std::string(describe_warning(1)), "Multipath Reply (Reflection)");
    CHECK_EQ(std::string(describe_warning(127)), "Reserved");

    CHECK_EQ(r.mode3a_confidence, 0x123);

    const auto& m = *r.mode_c;
    CHECK(m.v);
    CHECK(m.g);
    CHECK_EQ(m.code, 0x629);
    CHECK_EQ(m.low_quality, 0xABC);
    CHECK_EQ(m.altitude_ft(), 33000);

    CHECK_EQ(r.height_3d->height, -100);
    CHECK_EQ(r.height_3d->feet(), -2500.0);

    const auto& ds = *r.radial_doppler_speed;
    REQUIRE(ds.cal && ds.rds);
    CHECK(ds.cal->d);
    CHECK_EQ(ds.cal->cal, -5);
    REQUIRE(ds.rds->size() == 1);
    CHECK_EQ((*ds.rds)[0].dop, 0x0102);
    CHECK_EQ((*ds.rds)[0].amb, 0x0304);
    CHECK_EQ((*ds.rds)[0].frq, 0x0506);

    const auto& cap = *r.comms_acas_capability;
    CHECK_EQ(cap.com, CommCapability::Level5);
    CHECK_EQ(cap.stat, FlightStatus::NoAlertSpi);
    CHECK(cap.si);
    CHECK(!cap.mssc);
    CHECK(cap.arc);
    CHECK(!cap.aic);
    CHECK(cap.b1a);
    CHECK_EQ(cap.b1b, 0xA);

    CHECK_EQ(r.acas_resolution_advisory, 0x11223344556677ull);

    CHECK(!r.mode1->v);
    CHECK(r.mode1->g);
    CHECK(!r.mode1->l);
    CHECK_EQ(r.mode1->code, 0x16);
    CHECK_EQ(r.mode1->to_string(), "52");

    CHECK_EQ(r.mode2->to_string(), "1234");
    CHECK_EQ(r.mode1_confidence, 0x15);
    CHECK_EQ(r.mode2_confidence, 0xFFF);

    CHECK(r.special_purpose == (Bytes{1, 2, 3}));
    REQUIRE(r.reserved_expansion);
    CHECK(r.reserved_expansion->empty());
}

TEST(cat048_partial_compound_subfields) {
    // I048/130 with only SRR (#2) and APD (#7); I048/120 with only RDS, REP = 0.
    const auto r = single_record<cat048::Record>(block(48, {record({{7, hex("42 07 F8")}, {20, hex("40 00")}})}));
    const auto& c = *r.radar_plot_characteristics;
    CHECK(!c.srl);
    CHECK_EQ(c.srr, 7);
    CHECK(!c.sam && !c.prl && !c.pam && !c.rpd);
    CHECK_EQ(c.apd, -8);
    CHECK(!c.srl_deg());
    REQUIRE(r.radial_doppler_speed);
    CHECK(!r.radial_doppler_speed->cal);
    REQUIRE(r.radial_doppler_speed->rds);
    CHECK(r.radial_doppler_speed->rds->empty());
}

TEST(cat048_compound_with_empty_extension_octet) {
    // A presence octet with FX set followed by an all-zero octet is tolerated.
    const auto r = single_record<cat048::Record>(block(48, {record({{7, hex("41 00 09")}})}));
    CHECK_EQ(r.radar_plot_characteristics->srr, 9);
}

TEST(cat048_unknown_later_extensions_are_skipped) {
    // Seven I048/020 octets: the sixth has FX set, the seventh is not defined
    // by edition 1.32 and is ignored; decoding continues with the next item.
    const auto r = single_record<cat048::Record>(block(48, {record({{3, hex("41 01 01 01 01 01 FE")}, {4, hex("01 00 02 00")}})}));
    CHECK_EQ(r.target_report_descriptor->typ, DetectionType::SingleSsr);
    CHECK(r.target_report_descriptor->ext5);
    CHECK_EQ(r.measured_position->rho, 0x100);
}

TEST(cat048_empty_repetitions) {
    const auto r = single_record<cat048::Record>(block(48, {record({{10, hex("00")}})}));
    REQUIRE(r.bds_register_data);
    CHECK(r.bds_register_data->empty());
}

TEST(cat048_multiple_records_in_block) {
    const Bytes r1 = record({{1, hex("01 02")}, {11, hex("00 01")}});
    const Bytes r2 = record({{1, hex("01 02")}, {11, hex("00 02")}});
    const Bytes r3 = record({{1, hex("01 02")}, {11, hex("0F FF")}});
    const auto result = decode(block(48, {r1, r2, r3}));
    REQUIRE(result.ok());
    REQUIRE(result.blocks.size() == 1);
    REQUIRE(result.blocks[0].records.size() == 3);
    CHECK_EQ(std::get<cat048::Record>(result.blocks[0].records[0]).track_number, 1);
    CHECK_EQ(std::get<cat048::Record>(result.blocks[0].records[1]).track_number, 2);
    CHECK_EQ(std::get<cat048::Record>(result.blocks[0].records[2]).track_number, 4095);
}

TEST(cat048_icao_character_set) {
    // All 64 six-bit codes: A-Z, space and 0-9 are defined; code 0 (sent when
    // no identification is available) reads as a space; anything else is '?'.
    const std::string expected =
        " ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";
    for (unsigned first = 0; first < 64; first += 8) {
        std::uint64_t w = 0;
        for (unsigned i = 0; i < 8; ++i) w = (w << 6) | (first + i);
        Bytes item;
        for (int shift = 40; shift >= 0; shift -= 8) item.push_back(static_cast<std::uint8_t>(w >> shift));
        const auto r = single_record<cat048::Record>(block(48, {record({{9, item}})}));
        CHECK_EQ(*r.aircraft_identification, expected.substr(first, 8));
    }
}

TEST(cat048_enum_descriptions) {
    CHECK_EQ(std::string(to_string(DetectionType::SsrPsr)), "SSR + PSR detection");
    CHECK_EQ(std::string(to_string(FoeFri::NoReply)), "No reply");
    CHECK_EQ(std::string(to_string(TrackSensorType::SsrModeS)), "SSR/Mode S Track");
    CHECK_EQ(std::string(to_string(ClimbDescentMode::Climbing)), "Climbing");
    CHECK_EQ(std::string(to_string(CommCapability::Level5)), "Level 5 Transponder capability");
    CHECK_EQ(std::string(to_string(static_cast<CommCapability>(6))), "Not assigned");
    CHECK_EQ(std::string(to_string(FlightStatus::Unknown)), "Unknown");
    CHECK_EQ(std::string(describe_warning(36)), "IC Conflict detection possible-no conflict currently detected");
}
