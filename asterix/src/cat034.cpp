#include "asterix/cat034.hpp"
#include "detail.hpp"

#include <iterator>

namespace asterix::cat034 {

const char* to_string(MessageType v) noexcept {
    switch (v) {
        case MessageType::NorthMarker: return "North marker message";
        case MessageType::SectorCrossing: return "Sector crossing message";
        case MessageType::GeographicalFiltering: return "Geographical filtering message";
        case MessageType::JammingStrobe: return "Jamming strobe message";
        case MessageType::SolarStorm: return "Solar Storm Message";
        case MessageType::SsrJammingStrobe: return "SSR Jamming Strobe Message";
        case MessageType::ModeSJammingStrobe: return "Mode S Jamming Strobe Message";
    }
    return "Unknown";
}

const char* to_string(DataFilter v) noexcept {
    switch (v) {
        case DataFilter::Invalid: return "Invalid value";
        case DataFilter::Weather: return "Filter for Weather data";
        case DataFilter::JammingStrobe: return "Filter for Jamming Strobe";
        case DataFilter::Psr: return "Filter for PSR data";
        case DataFilter::SsrModeS: return "Filter for SSR/Mode S data";
        case DataFilter::SsrModeSPsr: return "Filter for SSR/Mode S + PSR data";
        case DataFilter::EnhancedSurveillance: return "Enhanced Surveillance data";
        case DataFilter::PsrEnhancedSurveillance: return "Filter for PSR+Enhanced Surveillance data";
        case DataFilter::PsrEnhancedSurveillanceSsrOutsideApi:
            return "Filter for PSR+Enhanced Surveillance + SSR/Mode S data not in Area of Prime Interest";
        case DataFilter::PsrEnhancedSurveillanceAllSsr:
            return "Filter for PSR+Enhanced Surveillance + all SSR/Mode S data";
    }
    return "Unknown";
}

const char* describe_counter(std::uint8_t typ) noexcept {
    static const char* const kText[] = {
        "No detection (number of misses)",
        "Single PSR target reports",
        "Single SSR target reports (Non-Mode S)",
        "SSR+PSR target reports (Non-Mode S)",
        "Single All-Call target reports (Mode S)",
        "Single Roll-Call target reports (Mode S)",
        "All-Call + PSR (Mode S) target reports",
        "Roll-Call + PSR (Mode S) target reports",
        "Filter for Weather data",
        "Filter for Jamming Strobe",
        "Filter for PSR data",
        "Filter for SSR/Mode S data",
        "Filter for SSR/Mode S+PSR data",
        "Filter for Enhanced Surveillance data",
        "Filter for PSR+Enhanced Surveillance",
        "Filter for PSR+Enhanced Surveillance + SSR/Mode S data not in Area of Prime Interest",
        "Filter for PSR+Enhanced Surveillance + all SSR/Mode S data",
        "Re-Interrogations (per sector)",
        "BDS Swap and wrong DF replies (per sector)",
        "Mode A/C FRUIT (per sector)",
        "Mode S FRUIT (per sector)",
    };
    return typ < std::size(kText) ? kText[typ] : "Reserved";
}

namespace {

using detail::bit;
using detail::ByteView;
using detail::Reader;
using detail::s8;
using detail::s16;
using detail::u8_bits;
using detail::u16_bits;

std::vector<std::uint8_t> explicit_payload(ByteView v) {
    Reader in(v);
    in.u8();  // length octet
    return in.rest();
}

void d010(ByteView v, Record& r) {
    Reader in(v);
    DataSourceId id;
    id.sac = in.u8();
    id.sic = in.u8();
    r.data_source = id;
}

void d000(ByteView v, Record& r) {
    Reader in(v);
    r.message_type = static_cast<MessageType>(in.u8());
}

void d030(ByteView v, Record& r) {
    Reader in(v);
    r.time_of_day = TimeOfDay{in.u24()};
}

void d020(ByteView v, Record& r) {
    Reader in(v);
    r.sector_number = SectorNumber{in.u8()};
}

void d041(ByteView v, Record& r) {
    Reader in(v);
    r.antenna_rotation_period = AntennaRotationPeriod{in.u16()};
}

// Subfield slots #2, #3 and #7 of I034/050 and I034/060 are spare.
const detail::Subfield kSubfields050[] = {
    detail::fixed_subfield(1),  // COM
    detail::spare_subfield(),
    detail::spare_subfield(),
    detail::fixed_subfield(1),  // PSR
    detail::fixed_subfield(1),  // SSR
    detail::fixed_subfield(2),  // MDS
};

void d050(ByteView v, Record& r) {
    Reader in(v);
    const std::uint32_t present = detail::read_compound_presence(in);
    const auto has = [present](unsigned subfield) { return ((present >> (subfield - 1)) & 1u) != 0; };
    SystemConfiguration s;
    if (has(1)) {
        const std::uint8_t o = in.u8();
        SystemConfiguration::Common c;
        c.nogo = bit(o, 8);
        c.rdpc = bit(o, 7);
        c.rdpr = bit(o, 6);
        c.ovl_rdp = bit(o, 5);
        c.ovl_xmt = bit(o, 4);
        c.msc = bit(o, 3);
        c.tsv = bit(o, 2);
        s.com = c;
    }
    if (has(4)) {
        const std::uint8_t o = in.u8();
        SystemConfiguration::Psr p;
        p.ant = bit(o, 8);
        p.chab = u8_bits(o, 7, 6);
        p.ovl = bit(o, 5);
        p.msc = bit(o, 4);
        s.psr = p;
    }
    if (has(5)) {
        const std::uint8_t o = in.u8();
        SystemConfiguration::Ssr p;
        p.ant = bit(o, 8);
        p.chab = u8_bits(o, 7, 6);
        p.ovl = bit(o, 5);
        p.msc = bit(o, 4);
        s.ssr = p;
    }
    if (has(6)) {
        const std::uint16_t w = in.u16();
        SystemConfiguration::ModeS m;
        m.ant = bit(w, 16);
        m.chab = u8_bits(w, 15, 14);
        m.ovl_sur = bit(w, 13);
        m.msc = bit(w, 12);
        m.scf = bit(w, 11);
        m.dlf = bit(w, 10);
        m.ovl_scf = bit(w, 9);
        m.ovl_dlf = bit(w, 8);
        s.mds = m;
    }
    r.system_configuration = s;
}

const detail::Subfield kSubfields060[] = {
    detail::fixed_subfield(1),  // COM
    detail::spare_subfield(),
    detail::spare_subfield(),
    detail::fixed_subfield(1),  // PSR
    detail::fixed_subfield(1),  // SSR
    detail::fixed_subfield(1),  // MDS
};

void d060(ByteView v, Record& r) {
    Reader in(v);
    const std::uint32_t present = detail::read_compound_presence(in);
    const auto has = [present](unsigned subfield) { return ((present >> (subfield - 1)) & 1u) != 0; };
    ProcessingMode m;
    if (has(1)) {
        const std::uint8_t o = in.u8();
        ProcessingMode::Common c;
        c.red_rdp = u8_bits(o, 7, 5);
        c.red_xmt = u8_bits(o, 4, 2);
        m.com = c;
    }
    if (has(4)) {
        const std::uint8_t o = in.u8();
        ProcessingMode::Psr p;
        p.pol = bit(o, 8);
        p.red_rad = u8_bits(o, 7, 5);
        p.stc = u8_bits(o, 4, 3);
        m.psr = p;
    }
    if (has(5)) {
        const std::uint8_t o = in.u8();
        ProcessingMode::Ssr p;
        p.red_rad = u8_bits(o, 8, 6);
        m.ssr = p;
    }
    if (has(6)) {
        const std::uint8_t o = in.u8();
        ProcessingMode::ModeS p;
        p.red_rad = u8_bits(o, 8, 6);
        p.clu = bit(o, 5);
        m.mds = p;
    }
    r.processing_mode = m;
}

void d070(ByteView v, Record& r) {
    Reader in(v);
    const std::size_t rep = in.u8();
    std::vector<MessageCount> counts;
    counts.reserve(rep);
    for (std::size_t i = 0; i < rep; ++i) {
        const std::uint16_t w = in.u16();
        MessageCount c;
        c.typ = u8_bits(w, 16, 12);
        c.count = u16_bits(w, 11, 1);
        counts.push_back(c);
    }
    r.message_count_values = std::move(counts);
}

void d100(ByteView v, Record& r) {
    Reader in(v);
    PolarWindow w;
    w.rho_start = in.u16();
    w.rho_end = in.u16();
    w.theta_start = in.u16();
    w.theta_end = in.u16();
    r.polar_window = w;
}

void d110(ByteView v, Record& r) {
    Reader in(v);
    r.data_filter = static_cast<DataFilter>(in.u8());
}

void d120(ByteView v, Record& r) {
    Reader in(v);
    Position3D p;
    p.height = s16(in.u16());
    p.lat = detail::sign_extend(in.u24(), 24);
    p.lon = detail::sign_extend(in.u24(), 24);
    r.position = p;
}

void d090(ByteView v, Record& r) {
    Reader in(v);
    CollimationError c;
    c.range = s8(in.u8());
    c.azimuth = s8(in.u8());
    r.collimation_error = c;
}

void dre(ByteView v, Record& r) {
    r.reserved_expansion = explicit_payload(v);
}

void dsp(ByteView v, Record& r) {
    r.special_purpose = explicit_payload(v);
}

using Def = detail::ItemDef<Record>;

// Standard UAP, edition 1.29.
const Def kUap[] = {
    Def::fixed("I034/010", 2, d010),                 // FRN 1
    Def::fixed("I034/000", 1, d000),                 // FRN 2
    Def::fixed("I034/030", 3, d030),                 // FRN 3
    Def::fixed("I034/020", 1, d020),                 // FRN 4
    Def::fixed("I034/041", 2, d041),                 // FRN 5
    Def::compound("I034/050", kSubfields050, d050),  // FRN 6
    Def::compound("I034/060", kSubfields060, d060),  // FRN 7
    Def::repetitive("I034/070", 2, d070),            // FRN 8
    Def::fixed("I034/100", 8, d100),                 // FRN 9
    Def::fixed("I034/110", 1, d110),                 // FRN 10
    Def::fixed("I034/120", 8, d120),                 // FRN 11
    Def::fixed("I034/090", 2, d090),                 // FRN 12
    Def::explicit_length("I034/RE", dre),            // FRN 13
    Def::explicit_length("I034/SP", dsp),            // FRN 14
};

}  // namespace
}  // namespace asterix::cat034

namespace asterix::detail {

void decode_cat034(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors) {
    decode_records(input, begin, end, 34, cat034::kUap, records, errors);
}

}  // namespace asterix::detail
