#include "asterix/cat048.hpp"
#include "detail.hpp"

#include <iterator>

namespace asterix::cat048 {

std::string Mode1Code::to_string() const {
    std::string out(2, '0');
    out[0] = static_cast<char>('0' + ((code >> 2) & 7u));
    out[1] = static_cast<char>('0' + (code & 3u));
    return out;
}

std::optional<double> RadarPlotCharacteristics::srl_deg() const noexcept {
    if (!srl) return std::nullopt;
    return *srl * (360.0 / 8192.0);
}

std::optional<double> RadarPlotCharacteristics::prl_deg() const noexcept {
    if (!prl) return std::nullopt;
    return *prl * (360.0 / 8192.0);
}

std::optional<double> RadarPlotCharacteristics::rpd_nm() const noexcept {
    if (!rpd) return std::nullopt;
    return *rpd / 256.0;
}

std::optional<double> RadarPlotCharacteristics::apd_deg() const noexcept {
    if (!apd) return std::nullopt;
    return *apd * (360.0 / 16384.0);
}

const char* to_string(DetectionType v) noexcept {
    switch (v) {
        case DetectionType::NoDetection: return "No detection";
        case DetectionType::SinglePsr: return "Single PSR detection";
        case DetectionType::SingleSsr: return "Single SSR detection";
        case DetectionType::SsrPsr: return "SSR + PSR detection";
        case DetectionType::SingleModeSAllCall: return "Single ModeS All-Call";
        case DetectionType::SingleModeSRollCall: return "Single ModeS Roll-Call";
        case DetectionType::ModeSAllCallPsr: return "ModeS All-Call + PSR";
        case DetectionType::ModeSRollCallPsr: return "ModeS Roll-Call + PSR";
    }
    return "Unknown";
}

const char* to_string(FoeFri v) noexcept {
    switch (v) {
        case FoeFri::NoMode4Interrogation: return "No Mode 4 interrogation";
        case FoeFri::FriendlyTarget: return "Friendly target";
        case FoeFri::UnknownTarget: return "Unknown target";
        case FoeFri::NoReply: return "No reply";
    }
    return "Unknown";
}

const char* to_string(TrackSensorType v) noexcept {
    switch (v) {
        case TrackSensorType::Combined: return "Combined Track";
        case TrackSensorType::Psr: return "PSR Track";
        case TrackSensorType::SsrModeS: return "SSR/Mode S Track";
        case TrackSensorType::Invalid: return "Invalid";
    }
    return "Unknown";
}

const char* to_string(ClimbDescentMode v) noexcept {
    switch (v) {
        case ClimbDescentMode::Maintaining: return "Maintaining";
        case ClimbDescentMode::Climbing: return "Climbing";
        case ClimbDescentMode::Descending: return "Descending";
        case ClimbDescentMode::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char* to_string(CommCapability v) noexcept {
    switch (v) {
        case CommCapability::SurveillanceOnly: return "No communications capability (surveillance only)";
        case CommCapability::CommAB: return "Comm. A and Comm. B capability";
        case CommCapability::CommABUplinkElm: return "Comm. A, Comm. B and Uplink ELM";
        case CommCapability::CommABUplinkDownlinkElm: return "Comm. A, Comm. B, Uplink ELM and Downlink ELM";
        case CommCapability::Level5: return "Level 5 Transponder capability";
    }
    return "Not assigned";
}

const char* to_string(FlightStatus v) noexcept {
    switch (v) {
        case FlightStatus::NoAlertNoSpiAirborne: return "No alert, no SPI, aircraft airborne";
        case FlightStatus::NoAlertNoSpiOnGround: return "No alert, no SPI, aircraft on ground";
        case FlightStatus::AlertNoSpiAirborne: return "Alert, no SPI, aircraft airborne";
        case FlightStatus::AlertNoSpiOnGround: return "Alert, no SPI, aircraft on ground";
        case FlightStatus::AlertSpi: return "Alert, SPI, aircraft airborne or on ground";
        case FlightStatus::NoAlertSpi: return "No alert, SPI, aircraft airborne or on ground";
        case FlightStatus::NotAssigned: return "Not assigned";
        case FlightStatus::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char* describe_warning(std::uint8_t code) noexcept {
    static const char* const kText[] = {
        "Not defined; never used",
        "Multipath Reply (Reflection)",
        "Reply due to sidelobe interrogation/reception",
        "Split plot",
        "Second time around reply",
        "Angel",
        "Slow moving target correlated with road infrastructure (terrestrial vehicle)",
        "Fixed PSR plot",
        "Slow PSR target",
        "Low quality PSR plot",
        "Phantom SSR plot",
        "Non-Matching Mode-3/A Code",
        "Mode C code / Mode S altitude code abnormal value compared to the track",
        "Target in Clutter Area",
        "Maximum Doppler Response in Zero Filter",
        "Transponder anomaly detected",
        "Duplicated or Illegal Mode S Aircraft Address",
        "Mode S error correction applied",
        "Undecodable Mode C code / Mode S altitude code",
        "Birds",
        "Flock of Birds",
        "Mode-1 was present in original reply",
        "Mode-2 was present in original reply",
        "Plot potentially caused by Wind Turbine",
        "Helicopter",
        "Maximum number of re-interrogations reached (surveillance information)",
        "Maximum number of re-interrogations reached (BDS Extractions)",
        "BDS Overlay Incoherence",
        "Potential BDS Swap Detected",
        "Track Update in the Zenithal Gap",
        "Mode S Track re-acquired",
        "Duplicated Mode 5 Pair NO/PIN detected",
        "Wrong DF reply format detected",
        "Transponder anomaly (MS XPD replies with Mode A/C to Mode A/C-only all-call)",
        "Transponder anomaly (SI capability report wrong)",
        "Potential IC Conflict",
        "IC Conflict detection possible-no conflict currently detected",
        "Duplicate Mode 5 PIN",
    };
    return code < std::size(kText) ? kText[code] : "Reserved";
}

namespace {

using detail::bit;
using detail::ByteView;
using detail::Reader;
using detail::s8;
using detail::s16;
using detail::u8_bits;
using detail::u16_bits;

PopulatedValue populated(std::uint8_t octet, unsigned ep_bit, unsigned val_hi, unsigned val_lo) {
    PopulatedValue p;
    p.ep = bit(octet, ep_bit);
    p.val = u8_bits(octet, val_hi, val_lo);
    return p;
}

OctalCode octal_code_item(ByteView v) {
    Reader in(v);
    const std::uint16_t w = in.u16();
    OctalCode c;
    c.v = bit(w, 16);
    c.g = bit(w, 15);
    c.l = bit(w, 14);
    c.code = u16_bits(w, 12, 1);
    return c;
}

std::vector<std::uint8_t> explicit_payload(ByteView v) {
    Reader in(v);
    in.u8();  // length octet
    return in.rest();
}

// ICAO 6-bit characters (Annex 10 Vol IV): 1-26 are A-Z, 32 is space and
// 48-57 are 0-9. Code 0 is sent by transponders without a flight
// identification and is shown as a space; other unused codes as '?'.
char icao_char(std::uint32_t c) {
    if (c >= 1 && c <= 26) return static_cast<char>('A' + (c - 1));
    if (c >= 48 && c <= 57) return static_cast<char>('0' + (c - 48));
    if (c == 0 || c == 32) return ' ';
    return '?';
}

void d010(ByteView v, Record& r) {
    Reader in(v);
    DataSourceId id;
    id.sac = in.u8();
    id.sic = in.u8();
    r.data_source = id;
}

void d140(ByteView v, Record& r) {
    Reader in(v);
    r.time_of_day = TimeOfDay{in.u24()};
}

void d020(ByteView v, Record& r) {
    const std::uint8_t* o = v.data;
    TargetReportDescriptor d;
    d.typ = static_cast<DetectionType>(u8_bits(o[0], 8, 6));
    d.sim = bit(o[0], 5);
    d.rdp = bit(o[0], 4);
    d.spi = bit(o[0], 3);
    d.rab = bit(o[0], 2);
    if (v.size > 1) {
        TargetReportDescriptor::Ext1 e;
        e.tst = bit(o[1], 8);
        e.err = bit(o[1], 7);
        e.xpp = bit(o[1], 6);
        e.me = bit(o[1], 5);
        e.mi = bit(o[1], 4);
        e.foe_fri = static_cast<FoeFri>(u8_bits(o[1], 3, 2));
        d.ext1 = e;
    }
    if (v.size > 2) {
        TargetReportDescriptor::Ext2 e;
        e.adsb = populated(o[2], 8, 7, 7);
        e.scn = populated(o[2], 6, 5, 5);
        e.pai = populated(o[2], 4, 3, 3);
        d.ext2 = e;
    }
    if (v.size > 3) {
        TargetReportDescriptor::Ext3 e;
        e.acasxv = populated(o[3], 8, 7, 4);
        e.poxpr = populated(o[3], 3, 2, 2);
        d.ext3 = e;
    }
    if (v.size > 4) {
        TargetReportDescriptor::Ext4 e;
        e.poact = populated(o[4], 8, 7, 7);
        e.dtfxpr = populated(o[4], 6, 5, 5);
        e.dtfact = populated(o[4], 4, 3, 3);
        d.ext4 = e;
    }
    if (v.size > 5) {
        TargetReportDescriptor::Ext5 e;
        e.irmxpr = populated(o[5], 8, 7, 7);
        e.irmact = populated(o[5], 6, 5, 5);
        d.ext5 = e;
    }
    // Extents beyond the fifth are not defined by edition 1.32 and are skipped.
    r.target_report_descriptor = d;
}

void d040(ByteView v, Record& r) {
    Reader in(v);
    PolarPosition p;
    p.rho = in.u16();
    p.theta = in.u16();
    r.measured_position = p;
}

void d070(ByteView v, Record& r) {
    r.mode3a = octal_code_item(v);
}

void d090(ByteView v, Record& r) {
    Reader in(v);
    const std::uint16_t w = in.u16();
    FlightLevel f;
    f.v = bit(w, 16);
    f.g = bit(w, 15);
    f.level = s16(detail::bits(w, 14, 1), 14);
    r.flight_level = f;
}

const detail::Subfield kSubfields130[] = {
    detail::fixed_subfield(1),  // SRL
    detail::fixed_subfield(1),  // SRR
    detail::fixed_subfield(1),  // SAM
    detail::fixed_subfield(1),  // PRL
    detail::fixed_subfield(1),  // PAM
    detail::fixed_subfield(1),  // RPD
    detail::fixed_subfield(1),  // APD
};

void d130(ByteView v, Record& r) {
    Reader in(v);
    const std::uint32_t present = detail::read_compound_presence(in);
    const auto has = [present](unsigned subfield) { return ((present >> (subfield - 1)) & 1u) != 0; };
    RadarPlotCharacteristics c;
    if (has(1)) c.srl = in.u8();
    if (has(2)) c.srr = in.u8();
    if (has(3)) c.sam = s8(in.u8());
    if (has(4)) c.prl = in.u8();
    if (has(5)) c.pam = s8(in.u8());
    if (has(6)) c.rpd = s8(in.u8());
    if (has(7)) c.apd = s8(in.u8());
    r.radar_plot_characteristics = c;
}

void d220(ByteView v, Record& r) {
    Reader in(v);
    r.aircraft_address = in.u24();
}

void d240(ByteView v, Record& r) {
    Reader in(v);
    const std::uint64_t w = in.be(6);
    std::string id(8, ' ');
    for (unsigned i = 0; i < 8; ++i) {
        id[i] = icao_char(detail::bits(w, 48 - 6 * i, 43 - 6 * i));
    }
    r.aircraft_identification = std::move(id);
}

void d250(ByteView v, Record& r) {
    Reader in(v);
    const std::size_t rep = in.u8();
    std::vector<BdsRegister> regs;
    regs.reserve(rep);
    for (std::size_t i = 0; i < rep; ++i) {
        BdsRegister b;
        b.mb_data = in.be(7);
        const std::uint8_t address = in.u8();
        b.bds1 = u8_bits(address, 8, 5);
        b.bds2 = u8_bits(address, 4, 1);
        regs.push_back(b);
    }
    r.bds_register_data = std::move(regs);
}

void d161(ByteView v, Record& r) {
    Reader in(v);
    r.track_number = u16_bits(in.u16(), 12, 1);
}

void d042(ByteView v, Record& r) {
    Reader in(v);
    CartesianPosition p;
    p.x = s16(in.u16());
    p.y = s16(in.u16());
    r.calculated_position = p;
}

void d200(ByteView v, Record& r) {
    Reader in(v);
    PolarVelocity p;
    p.gsp = in.u16();
    p.hdg = in.u16();
    r.calculated_velocity = p;
}

void d170(ByteView v, Record& r) {
    const std::uint8_t* o = v.data;
    TrackStatus s;
    s.cnf = bit(o[0], 8);
    s.rad = static_cast<TrackSensorType>(u8_bits(o[0], 7, 6));
    s.dou = bit(o[0], 5);
    s.mah = bit(o[0], 4);
    s.cdm = static_cast<ClimbDescentMode>(u8_bits(o[0], 3, 2));
    if (v.size > 1) {
        TrackStatus::Ext1 e;
        e.tre = bit(o[1], 8);
        e.gho = bit(o[1], 7);
        e.sup = bit(o[1], 6);
        e.tcc = bit(o[1], 5);
        s.ext1 = e;
    }
    r.track_status = s;
}

void d210(ByteView v, Record& r) {
    Reader in(v);
    TrackQuality q;
    q.sigx = in.u8();
    q.sigy = in.u8();
    q.sigv = in.u8();
    q.sigh = in.u8();
    r.track_quality = q;
}

void d030(ByteView v, Record& r) {
    std::vector<std::uint8_t> codes;
    codes.reserve(v.size);
    for (std::size_t i = 0; i < v.size; ++i) {
        codes.push_back(u8_bits(v.data[i], 8, 2));
    }
    r.warning_error_conditions = std::move(codes);
}

void d080(ByteView v, Record& r) {
    Reader in(v);
    r.mode3a_confidence = u16_bits(in.u16(), 12, 1);
}

void d100(ByteView v, Record& r) {
    Reader in(v);
    const std::uint32_t w = in.u32();
    ModeCCode m;
    m.v = bit(w, 32);
    m.g = bit(w, 31);
    m.code = u16_bits(w, 28, 17);
    m.low_quality = u16_bits(w, 12, 1);
    r.mode_c = m;
}

void d110(ByteView v, Record& r) {
    Reader in(v);
    Height3D h;
    h.height = s16(detail::bits(in.u16(), 14, 1), 14);
    r.height_3d = h;
}

const detail::Subfield kSubfields120[] = {
    detail::fixed_subfield(2),       // CAL
    detail::repetitive_subfield(6),  // RDS
};

void d120(ByteView v, Record& r) {
    Reader in(v);
    const std::uint32_t present = detail::read_compound_presence(in);
    RadialDopplerSpeed s;
    if ((present & 1u) != 0) {
        const std::uint16_t w = in.u16();
        RadialDopplerSpeed::Calculated c;
        c.d = bit(w, 16);
        c.cal = s16(detail::bits(w, 10, 1), 10);
        s.cal = c;
    }
    if ((present & 2u) != 0) {
        const std::size_t rep = in.u8();
        std::vector<RadialDopplerSpeed::Raw> raw;
        raw.reserve(rep);
        for (std::size_t i = 0; i < rep; ++i) {
            RadialDopplerSpeed::Raw x;
            x.dop = in.u16();
            x.amb = in.u16();
            x.frq = in.u16();
            raw.push_back(x);
        }
        s.rds = std::move(raw);
    }
    r.radial_doppler_speed = std::move(s);
}

void d230(ByteView v, Record& r) {
    Reader in(v);
    const std::uint16_t w = in.u16();
    CommsAcasCapability c;
    c.com = static_cast<CommCapability>(u8_bits(w, 16, 14));
    c.stat = static_cast<FlightStatus>(u8_bits(w, 13, 11));
    c.si = bit(w, 10);
    c.mssc = bit(w, 8);
    c.arc = bit(w, 7);
    c.aic = bit(w, 6);
    c.b1a = bit(w, 5);
    c.b1b = u8_bits(w, 4, 1);
    r.comms_acas_capability = c;
}

void d260(ByteView v, Record& r) {
    Reader in(v);
    r.acas_resolution_advisory = in.be(7);
}

void d055(ByteView v, Record& r) {
    Reader in(v);
    const std::uint8_t o = in.u8();
    Mode1Code m;
    m.v = bit(o, 8);
    m.g = bit(o, 7);
    m.l = bit(o, 6);
    m.code = u8_bits(o, 5, 1);
    r.mode1 = m;
}

void d050(ByteView v, Record& r) {
    r.mode2 = octal_code_item(v);
}

void d065(ByteView v, Record& r) {
    Reader in(v);
    r.mode1_confidence = u8_bits(in.u8(), 5, 1);
}

void d060(ByteView v, Record& r) {
    Reader in(v);
    r.mode2_confidence = u16_bits(in.u16(), 12, 1);
}

void dsp(ByteView v, Record& r) {
    r.special_purpose = explicit_payload(v);
}

void dre(ByteView v, Record& r) {
    r.reserved_expansion = explicit_payload(v);
}

using Def = detail::ItemDef<Record>;

// Standard UAP, edition 1.32.
const Def kUap[] = {
    Def::fixed("I048/010", 2, d010),                     // FRN 1
    Def::fixed("I048/140", 3, d140),                     // FRN 2
    Def::extended("I048/020", d020),                     // FRN 3
    Def::fixed("I048/040", 4, d040),                     // FRN 4
    Def::fixed("I048/070", 2, d070),                     // FRN 5
    Def::fixed("I048/090", 2, d090),                     // FRN 6
    Def::compound("I048/130", kSubfields130, d130),      // FRN 7
    Def::fixed("I048/220", 3, d220),                     // FRN 8
    Def::fixed("I048/240", 6, d240),                     // FRN 9
    Def::repetitive("I048/250", 8, d250),                // FRN 10
    Def::fixed("I048/161", 2, d161),                     // FRN 11
    Def::fixed("I048/042", 4, d042),                     // FRN 12
    Def::fixed("I048/200", 4, d200),                     // FRN 13
    Def::extended("I048/170", d170),                     // FRN 14
    Def::fixed("I048/210", 4, d210),                     // FRN 15
    Def::extended("I048/030", d030),                     // FRN 16
    Def::fixed("I048/080", 2, d080),                     // FRN 17
    Def::fixed("I048/100", 4, d100),                     // FRN 18
    Def::fixed("I048/110", 2, d110),                     // FRN 19
    Def::compound("I048/120", kSubfields120, d120),      // FRN 20
    Def::fixed("I048/230", 2, d230),                     // FRN 21
    Def::fixed("I048/260", 7, d260),                     // FRN 22
    Def::fixed("I048/055", 1, d055),                     // FRN 23
    Def::fixed("I048/050", 2, d050),                     // FRN 24
    Def::fixed("I048/065", 1, d065),                     // FRN 25
    Def::fixed("I048/060", 2, d060),                     // FRN 26
    Def::explicit_length("I048/SP", dsp),                // FRN 27
    Def::explicit_length("I048/RE", dre),                // FRN 28
};

}  // namespace
}  // namespace asterix::cat048

namespace asterix::detail {

void decode_cat048(const std::uint8_t* input, std::size_t begin, std::size_t end,
                   std::vector<Record>& records, std::vector<DecodeError>& errors) {
    decode_records(input, begin, end, 48, cat048::kUap, records, errors);
}

}  // namespace asterix::detail
