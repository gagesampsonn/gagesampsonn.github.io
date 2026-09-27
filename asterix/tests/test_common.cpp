#include "helpers.hpp"

using namespace test;
using namespace asterix;

namespace {

// Independent Gillham encoder: altitude (feet, multiple of 100) to the 12-bit
// I048/100 reply order C1 A1 C2 A2 C4 A4 B1 D1 B2 D2 B4 D4.
std::uint16_t gillham_encode(int feet) {
    const int steps = feet / 100 + 12;  // 100 ft steps above -1200 ft
    const unsigned band = static_cast<unsigned>(steps / 5);
    unsigned hundreds = static_cast<unsigned>(steps % 5) + 1;
    if (band % 2 == 1) hundreds = 6 - hundreds;  // reflected in odd bands
    // C1 C2 C4 patterns for 100 ft steps 1..5.
    static const unsigned kC[6][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}};
    const unsigned c1 = kC[hundreds][0], c2 = kC[hundreds][1], c4 = kC[hundreds][2];
    const unsigned gray = band ^ (band >> 1);  // D2 D4 A1 A2 A4 B1 B2 B4, MSB first
    const unsigned d2 = (gray >> 7) & 1, d4 = (gray >> 6) & 1, a1 = (gray >> 5) & 1, a2 = (gray >> 4) & 1;
    const unsigned a4 = (gray >> 3) & 1, b1 = (gray >> 2) & 1, b2 = (gray >> 1) & 1, b4 = gray & 1;
    const unsigned order[12] = {c1, a1, c2, a2, c4, a4, b1, 0 /* D1 */, b2, d2, b4, d4};
    unsigned code = 0;
    for (const unsigned bit : order) code = (code << 1) | bit;
    return static_cast<std::uint16_t>(code);
}

}  // namespace

TEST(mode_c_known_codes) {
    CHECK_EQ(decode_mode_c_altitude(0x629), 33000);  // A1 B1 B2 C2 D4
    CHECK_EQ(decode_mode_c_altitude(0x080), -1200);  // C4 only: lowest altitude
    CHECK(!decode_mode_c_altitude(0x000));           // no C pulse
    CHECK(!decode_mode_c_altitude(0x880));           // C1 + C4: not a valid 100 ft code
    CHECK(!decode_mode_c_altitude(0xAA8));           // C1 + C2 + C4
}

TEST(mode_c_gillham_round_trip) {
    // Every altitude the Gillham code can express, -1200 ft to 126700 ft.
    int checked = 0;
    for (int feet = -1200; feet <= 126700; feet += 100) {
        const std::uint16_t code = gillham_encode(feet);
        const auto decoded = decode_mode_c_altitude(code);
        if (!decoded || *decoded != feet) {
            CHECK_EQ(decoded, feet);
            break;
        }
        ++checked;
    }
    CHECK_EQ(checked, 1280);
}

TEST(mode_c_mode_s_q_bit) {
    // D1 (Q) set: 11-bit binary count of 25 ft steps above -1000 ft.
    CHECK_EQ(decode_mode_c_altitude(0x010), -1000);  // N = 0
    const unsigned n = 1000;                         // 24000 ft
    const auto code = static_cast<std::uint16_t>(((n >> 4) << 5) | 0x10u | (n & 0xFu));
    CHECK_EQ(decode_mode_c_altitude(code), 24000);
    CHECK_EQ(decode_mode_c_altitude(0xFFF), 2047 * 25 - 1000);
}

TEST(octal_code_formatting) {
    CHECK_EQ(octal_code(0), "0000");
    CHECK_EQ(octal_code(0xFFF), "7777");
    CHECK_EQ(octal_code(01234), "1234");
    CHECK_EQ(octal_code(07500), "7500");
}

TEST(time_of_day_formatting) {
    CHECK_EQ(TimeOfDay{0}.to_string(), "00:00:00.000");
    CHECK_EQ(TimeOfDay{127}.to_string(), "00:00:00.992");
    CHECK_EQ(TimeOfDay{128u * 3661u}.to_string(), "01:01:01.000");
    CHECK_EQ(TimeOfDay{128u * 86400u - 1u}.to_string(), "23:59:59.992");
    CHECK_EQ(TimeOfDay{0xFFFFFFu}.to_string(), "36:24:31.992");  // out of range values still format
    CHECK_EQ(TimeOfDay{64}.seconds(), 0.5);
}

TEST(populated_value) {
    PopulatedValue p;
    CHECK(!p.value());
    p.val = 1;
    CHECK(!p.value());
    p.ep = true;
    CHECK_EQ(p.value(), 1);
}

TEST(data_source_equality) {
    CHECK((DataSourceId{1, 2} == DataSourceId{1, 2}));
    CHECK((DataSourceId{1, 2} != DataSourceId{2, 1}));
}

TEST(library_version) {
    CHECK_EQ(std::string(version()), "1.0.0");
}
