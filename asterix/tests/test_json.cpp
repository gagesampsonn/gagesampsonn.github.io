#include "helpers.hpp"

using namespace test;
using namespace asterix;

TEST(json_published_cat048_example) {
    const auto result = decode(cat048_example());
    REQUIRE(result.ok() && result.blocks.size() == 1 && result.blocks[0].records.size() == 1);
    const std::string expected =
        R"({"I010":{"SAC":25,"SIC":201},"I140":27354.6015625,)"
        R"("I020":{"TYP":5,"SIM":0,"RDP":0,"SPI":0,"RAB":0},)"
        R"("I040":{"RHO":197.68359375,"THETA":340.13671875},)"
        R"("I070":{"V":0,"G":0,"L":0,"MODE3A":"1000"},"I090":{"V":0,"G":0,"FL":330},)"
        R"("I220":"3C660C","I240":"DLH65A  ","I250":[{"MBDATA":"C0780031BC0000","BDS1":4,"BDS2":0}],)"
        R"("I161":{"TRN":3563},"I200":{"GSP":0.12066650390625,"HDG":124.002685546875},)"
        R"("I170":{"CNF":0,"RAD":2,"DOU":0,"MAH":0,"CDM":0,"TRE":0,"GHO":0,"SUP":0,"TCC":0},)"
        R"("I230":{"COM":1,"STAT":0,"SI":0,"MSSC":1,"ARC":1,"AIC":1,"B1A":1,"B1B":5}})";
    CHECK_EQ(to_json(result.blocks[0].records[0]), expected);
    CHECK_EQ(to_json(result.blocks[0]), R"({"category":48,"offset":0,"length":48,"records":[)" + expected + "]}");
}

TEST(json_cat034_sample) {
    const auto result = decode(cat034_example());
    REQUIRE(result.ok());
    CHECK_EQ(to_json(result.blocks[0].records[0]),
             R"({"I010":{"SAC":25,"SIC":14},"I000":2,"I030":29906.3359375,"I020":90,)"
             R"("I050":{"COM":{"NOGO":0,"RDPC":1,"RDPR":0,"OVLRDP":0,"OVLXMT":0,"MSC":0,"TSV":0},)"
             R"("SSR":{"ANT":0,"CHAB":2,"OVL":0,"MSC":0}},"I060":{"COM":{"REDRDP":0,"REDXMT":0}}})");
}

TEST(json_cat063_record) {
    const auto result = decode(block(63, {record({{1, hex("05 06")}, {4, hex("07 08")}, {5, hex("41 20")},
                                                   {6, hex("00 0A")}, {8, hex("FF FF")}, {14, hex("02 AB")}})}));
    REQUIRE(result.ok());
    CHECK_EQ(to_json(result.blocks[0].records[0]),
             R"({"I010":{"SAC":5,"SIC":6},"I050":{"SAC":7,"SIC":8},)"
             R"("I060":{"CON":1,"PSR":0,"SSR":0,"MDS":0,"ADS":0,"MLT":0,"OPS":0,"ODP":0,"OXT":1,"MSC":0,"TSV":0,"NPW":0},)"
             R"("I070":10,"I081":-0.0054931640625,"SP":"AB"})");
}

TEST(json_errors_and_unsupported_blocks) {
    const auto result = decode(concat({hex("01 00 04 00"), hex("30 00 05 81 01")}));
    CHECK_EQ(to_json(result),
             R"({"blocks":[{"category":1,"offset":0,"length":4,"supported":false,"records":[]},)"
             R"({"category":48,"offset":4,"length":5,"records":[]}],)"
             R"("errors":[{"code":"TruncatedRecord","offset":9,"category":48,"item":"FSPEC",)"
             R"("message":"FSPEC runs past the end of the data block"}]})");
}

TEST(json_writer_escaping) {
    std::string out;
    json::Writer w(out);
    w.begin_array();
    w.value(std::string("q\"b\\n\n\x01\t"));
    w.value(std::string("\xC3\xA9"));
    w.value(true).value(false).null();
    w.value(-5).value(18446744073709551615ull);
    w.end_array();
    CHECK_EQ(out, R"(["q\"b\\n\n\u0001\t","\u00C3\u00A9",true,false,null,-5,18446744073709551615])");
}

TEST(json_writer_numbers) {
    std::string out;
    json::Writer w(out);
    w.begin_array().value(0.1).value(330.0).value(-0.0078125).value(1e-5).value(1.0 / 3.0).end_array();
    CHECK_EQ(out, "[0.1,330,-0.0078125,1e-05,0.3333333333333333]");
}

TEST(json_writer_pretty) {
    std::string out;
    json::Writer w(out, 2);
    w.begin_object();
    w.key("a").value(1);
    w.key("b").begin_array().value(1).value(2).end_array();
    w.key("c").begin_object().end_object();
    w.key("d").begin_array().end_array();
    w.end_object();
    CHECK_EQ(out, "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    2\n  ],\n  \"c\": {},\n  \"d\": []\n}");
}

TEST(summary_lines) {
    const auto r048 = decode(cat048_example());
    CHECK_EQ(summarize(r048.blocks[0].records[0]),
             "CAT048 25/201 07:35:54.602 ModeS-RC rho=197.684NM theta=340.137deg A=1000 FL=330.00 "
             "addr=3C660C id=DLH65A trk=3563 gs=434.4kt hdg=124.00deg bds=4,0");
    const auto r034 = decode(cat034_example());
    CHECK_EQ(summarize(r034.blocks[0].records[0]), "CAT034 25/14 08:18:26.336 SectorCrossing sector=90.00deg rdpc=2");
}
