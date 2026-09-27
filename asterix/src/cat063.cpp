#include "asterix/cat063.hpp"
#include "detail.hpp"

namespace asterix::cat063 {

const char* to_string(ConnectionStatus v) noexcept {
    switch (v) {
        case ConnectionStatus::Operational: return "Operational";
        case ConnectionStatus::Degraded: return "Degraded";
        case ConnectionStatus::Initialization: return "Initialization";
        case ConnectionStatus::NotConnected: return "Not currently connected";
    }
    return "Unknown";
}

namespace {

using detail::bit;
using detail::ByteView;
using detail::Reader;
using detail::s16;
using detail::u8_bits;

std::vector<std::uint8_t> explicit_payload(ByteView v) {
    Reader in(v);
    in.u8();  // length octet
    return in.rest();
}

DataSourceId source_id(ByteView v) {
    Reader in(v);
    DataSourceId id;
    id.sac = in.u8();
    id.sic = in.u8();
    return id;
}

RangeGainBias range_gain_bias(ByteView v) {
    Reader in(v);
    RangeGainBias g;
    g.gain = s16(in.u16());
    g.bias = s16(in.u16());
    return g;
}

AngleBias angle_bias(ByteView v) {
    Reader in(v);
    return AngleBias{s16(in.u16())};
}

void d010(ByteView v, Record& r) {
    r.data_source = source_id(v);
}

void d015(ByteView v, Record& r) {
    Reader in(v);
    r.service_identification = in.u8();
}

void d030(ByteView v, Record& r) {
    Reader in(v);
    r.time_of_message = TimeOfDay{in.u24()};
}

void d050(ByteView v, Record& r) {
    r.sensor = source_id(v);
}

void d060(ByteView v, Record& r) {
    const std::uint8_t* o = v.data;
    SensorStatus s;
    s.con = static_cast<ConnectionStatus>(u8_bits(o[0], 8, 7));
    s.psr = bit(o[0], 6);
    s.ssr = bit(o[0], 5);
    s.mds = bit(o[0], 4);
    s.ads = bit(o[0], 3);
    s.mlt = bit(o[0], 2);
    if (v.size > 1) {
        SensorStatus::Ext1 e;
        e.ops = bit(o[1], 8);
        e.odp = bit(o[1], 7);
        e.oxt = bit(o[1], 6);
        e.msc = bit(o[1], 5);
        e.tsv = bit(o[1], 4);
        e.npw = bit(o[1], 3);
        s.ext1 = e;
    }
    if (v.size > 2) {
        SensorStatus::Ext2 e;
        e.ttf.ep = bit(o[2], 8);
        e.ttf.val = u8_bits(o[2], 7, 7);
        e.spo.ep = bit(o[2], 6);
        e.spo.val = u8_bits(o[2], 5, 5);
        s.ext2 = e;
    }
    // Extents beyond the second are not defined by edition 1.7 and are skipped.
    r.sensor_status = s;
}

void d070(ByteView v, Record& r) {
    Reader in(v);
    r.time_stamping_bias_ms = s16(in.u16());
}

void d080(ByteView v, Record& r) {
    r.ssr_range_gain_bias = range_gain_bias(v);
}

void d081(ByteView v, Record& r) {
    r.ssr_azimuth_bias = angle_bias(v);
}

void d090(ByteView v, Record& r) {
    r.psr_range_gain_bias = range_gain_bias(v);
}

void d091(ByteView v, Record& r) {
    r.psr_azimuth_bias = angle_bias(v);
}

void d092(ByteView v, Record& r) {
    r.psr_elevation_bias = angle_bias(v);
}

void dre(ByteView v, Record& r) {
    r.reserved_expansion = explicit_payload(v);
}

void dsp(ByteView v, Record& r) {
    r.special_purpose = explicit_payload(v);
}

using Def = detail::ItemDef<Record>;

// Standard UAP, edition 1.7.
const Def kUap[] = {
    Def::fixed("I063/010", 2, d010),       // FRN 1
    Def::fixed("I063/015", 1, d015),       // FRN 2
    Def::fixed("I063/030", 3, d030),       // FRN 3
    Def::fixed("I063/050", 2, d050),       // FRN 4
    Def::extended("I063/060", d060),       // FRN 5
    Def::fixed("I063/070", 2, d070),       // FRN 6
    Def::fixed("I063/080", 4, d080),       // FRN 7
    Def::fixed("I063/081", 2, d081),       // FRN 8
    Def::fixed("I063/090", 4, d090),       // FRN 9
    Def::fixed("I063/091", 2, d091),       // FRN 10
    Def::fixed("I063/092", 2, d092),       // FRN 11
    Def::spare(),                          // FRN 12
    Def::explicit_length("I063/RE", dre),  // FRN 13
    Def::explicit_length("I063/SP", dsp),  // FRN 14
};

}  // namespace
}  // namespace asterix::cat063

namespace asterix::detail {

void decode_cat063(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors) {
    decode_records(input, begin, end, 63, cat063::kUap, records, errors);
}

}  // namespace asterix::detail
