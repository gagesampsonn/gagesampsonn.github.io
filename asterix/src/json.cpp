#include "asterix/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace asterix::json {

namespace {

const char kHexDigits[] = "0123456789ABCDEF";

void append_escaped(std::string& out, std::string_view text) {
    out += '"';
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20 || c >= 0x7F) {
                    // Control characters, and bytes that are not ASCII so the
                    // output stays valid UTF-8 whatever the input.
                    out += "\\u00";
                    out += kHexDigits[c >> 4];
                    out += kHexDigits[c & 0xF];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

// Shortest decimal representation that reads back as the same double.
void append_double(std::string& out, double v) {
    if (!std::isfinite(v)) {
        out += "null";
        return;
    }
    char buf[64];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    const auto res = std::to_chars(buf, buf + sizeof buf, v);
    out.append(buf, res.ptr);
#else
    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof buf, "%.*g", precision, v);
        if (std::strtod(buf, nullptr) == v) {
            break;
        }
    }
    for (char* p = buf; *p != '\0'; ++p) {
        if (*p == ',') *p = '.';  // locales with a decimal comma
    }
    out += buf;
#endif
}

std::string hex(std::uint64_t v, unsigned digits) {
    std::string s(digits, '0');
    for (unsigned i = digits; i-- > 0;) {
        s[i] = kHexDigits[v & 0xF];
        v >>= 4;
    }
    return s;
}

std::string hex(const std::vector<std::uint8_t>& bytes) {
    std::string s;
    s.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes) {
        s += kHexDigits[b >> 4];
        s += kHexDigits[b & 0xF];
    }
    return s;
}

void flag(Writer& w, const char* name, bool b) {
    w.key(name).value(b ? 1 : 0);
}

template <class E>
void code(Writer& w, const char* name, E e) {
    w.key(name).value(static_cast<unsigned>(e));
}

void source(Writer& w, const char* name, const DataSourceId& id) {
    w.key(name).begin_object().key("SAC").value(id.sac).key("SIC").value(id.sic).end_object();
}

void populated(Writer& w, const char* name, const PopulatedValue& p) {
    w.key(name).begin_object();
    flag(w, "EP", p.ep);
    w.key("VAL").value(p.val);
    w.end_object();
}

// One member per bit of `mask`, most significant first.
template <std::size_t N>
void bit_members(Writer& w, std::uint32_t mask, const char* const (&names)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        flag(w, names[i], ((mask >> (N - 1 - i)) & 1u) != 0);
    }
}

const char* const kQuality12[] = {"QA4", "QA2", "QA1", "QB4", "QB2", "QB1",
                                  "QC4", "QC2", "QC1", "QD4", "QD2", "QD1"};
const char* const kQualityModeC[] = {"QC1", "QA1", "QC2", "QA2", "QC4", "QA4",
                                     "QB1", "QD1", "QB2", "QD2", "QB4", "QD4"};
const char* const kQualityMode1[] = {"QA4", "QA2", "QA1", "QB2", "QB1"};

void octal_code(Writer& w, const char* item, const char* name, const cat048::OctalCode& c) {
    w.key(item).begin_object();
    flag(w, "V", c.v);
    flag(w, "G", c.g);
    flag(w, "L", c.l);
    w.key(name).value(c.to_string());
    w.end_object();
}

}  // namespace

Writer::Writer(std::string& out, int indent) : out_(out), indent_(indent) {}

void Writer::newline() {
    if (indent_ < 0) {
        return;
    }
    out_ += '\n';
    out_.append(stack_.size() * static_cast<std::size_t>(indent_), ' ');
}

void Writer::before_value() {
    if (after_key_) {
        after_key_ = false;
        return;
    }
    if (!stack_.empty()) {
        if (!stack_.back().empty) {
            out_ += ',';
        }
        stack_.back().empty = false;
        newline();
    }
}

Writer& Writer::begin_object() {
    before_value();
    out_ += '{';
    stack_.push_back(Level{});
    return *this;
}

Writer& Writer::end_object() {
    const bool empty = stack_.empty() || stack_.back().empty;
    if (!stack_.empty()) {
        stack_.pop_back();
    }
    if (!empty) {
        newline();
    }
    out_ += '}';
    return *this;
}

Writer& Writer::begin_array() {
    before_value();
    out_ += '[';
    stack_.push_back(Level{});
    return *this;
}

Writer& Writer::end_array() {
    const bool empty = stack_.empty() || stack_.back().empty;
    if (!stack_.empty()) {
        stack_.pop_back();
    }
    if (!empty) {
        newline();
    }
    out_ += ']';
    return *this;
}

Writer& Writer::key(std::string_view name) {
    before_value();
    append_escaped(out_, name);
    out_ += indent_ < 0 ? ":" : ": ";
    after_key_ = true;
    return *this;
}

Writer& Writer::value(std::string_view text) {
    before_value();
    append_escaped(out_, text);
    return *this;
}

Writer& Writer::value(bool b) {
    before_value();
    out_ += b ? "true" : "false";
    return *this;
}

Writer& Writer::value(double number) {
    before_value();
    append_double(out_, number);
    return *this;
}

Writer& Writer::null() {
    before_value();
    out_ += "null";
    return *this;
}

Writer& Writer::signed_value(long long number) {
    before_value();
    out_ += std::to_string(number);
    return *this;
}

Writer& Writer::unsigned_value(unsigned long long number) {
    before_value();
    out_ += std::to_string(number);
    return *this;
}

void write(Writer& w, const cat048::Record& r) {
    w.begin_object();
    if (r.data_source) {
        source(w, "I010", *r.data_source);
    }
    if (r.time_of_day) {
        w.key("I140").value(r.time_of_day->seconds());
    }
    if (r.target_report_descriptor) {
        const auto& d = *r.target_report_descriptor;
        w.key("I020").begin_object();
        code(w, "TYP", d.typ);
        flag(w, "SIM", d.sim);
        flag(w, "RDP", d.rdp);
        flag(w, "SPI", d.spi);
        flag(w, "RAB", d.rab);
        if (d.ext1) {
            flag(w, "TST", d.ext1->tst);
            flag(w, "ERR", d.ext1->err);
            flag(w, "XPP", d.ext1->xpp);
            flag(w, "ME", d.ext1->me);
            flag(w, "MI", d.ext1->mi);
            code(w, "FOEFRI", d.ext1->foe_fri);
        }
        if (d.ext2) {
            populated(w, "ADSB", d.ext2->adsb);
            populated(w, "SCN", d.ext2->scn);
            populated(w, "PAI", d.ext2->pai);
        }
        if (d.ext3) {
            populated(w, "ACASXV", d.ext3->acasxv);
            populated(w, "POXPR", d.ext3->poxpr);
        }
        if (d.ext4) {
            populated(w, "POACT", d.ext4->poact);
            populated(w, "DTFXPR", d.ext4->dtfxpr);
            populated(w, "DTFACT", d.ext4->dtfact);
        }
        if (d.ext5) {
            populated(w, "IRMXPR", d.ext5->irmxpr);
            populated(w, "IRMACT", d.ext5->irmact);
        }
        w.end_object();
    }
    if (r.measured_position) {
        const auto& p = *r.measured_position;
        w.key("I040").begin_object().key("RHO").value(p.rho_nm()).key("THETA").value(p.theta_deg()).end_object();
    }
    if (r.mode3a) {
        octal_code(w, "I070", "MODE3A", *r.mode3a);
    }
    if (r.flight_level) {
        const auto& f = *r.flight_level;
        w.key("I090").begin_object();
        flag(w, "V", f.v);
        flag(w, "G", f.g);
        w.key("FL").value(f.fl());
        w.end_object();
    }
    if (r.radar_plot_characteristics) {
        const auto& c = *r.radar_plot_characteristics;
        w.key("I130").begin_object();
        if (c.srl) w.key("SRL").value(*c.srl_deg());
        if (c.srr) w.key("SRR").value(*c.srr);
        if (c.sam) w.key("SAM").value(*c.sam);
        if (c.prl) w.key("PRL").value(*c.prl_deg());
        if (c.pam) w.key("PAM").value(*c.pam);
        if (c.rpd) w.key("RPD").value(*c.rpd_nm());
        if (c.apd) w.key("APD").value(*c.apd_deg());
        w.end_object();
    }
    if (r.aircraft_address) {
        w.key("I220").value(hex(*r.aircraft_address, 6));
    }
    if (r.aircraft_identification) {
        w.key("I240").value(*r.aircraft_identification);
    }
    if (r.bds_register_data) {
        w.key("I250").begin_array();
        for (const auto& b : *r.bds_register_data) {
            w.begin_object();
            w.key("MBDATA").value(hex(b.mb_data, 14));
            w.key("BDS1").value(b.bds1);
            w.key("BDS2").value(b.bds2);
            w.end_object();
        }
        w.end_array();
    }
    if (r.track_number) {
        w.key("I161").begin_object().key("TRN").value(*r.track_number).end_object();
    }
    if (r.calculated_position) {
        const auto& p = *r.calculated_position;
        w.key("I042").begin_object().key("X").value(p.x_nm()).key("Y").value(p.y_nm()).end_object();
    }
    if (r.calculated_velocity) {
        const auto& v = *r.calculated_velocity;
        w.key("I200").begin_object();
        w.key("GSP").value(v.groundspeed_nm_s());
        w.key("HDG").value(v.heading_deg());
        w.end_object();
    }
    if (r.track_status) {
        const auto& s = *r.track_status;
        w.key("I170").begin_object();
        flag(w, "CNF", s.cnf);
        code(w, "RAD", s.rad);
        flag(w, "DOU", s.dou);
        flag(w, "MAH", s.mah);
        code(w, "CDM", s.cdm);
        if (s.ext1) {
            flag(w, "TRE", s.ext1->tre);
            flag(w, "GHO", s.ext1->gho);
            flag(w, "SUP", s.ext1->sup);
            flag(w, "TCC", s.ext1->tcc);
        }
        w.end_object();
    }
    if (r.track_quality) {
        const auto& q = *r.track_quality;
        w.key("I210").begin_object();
        w.key("SIGX").value(q.sigma_x_nm());
        w.key("SIGY").value(q.sigma_y_nm());
        w.key("SIGV").value(q.sigma_v_nm_s());
        w.key("SIGH").value(q.sigma_h_deg());
        w.end_object();
    }
    if (r.warning_error_conditions) {
        w.key("I030").begin_array();
        for (const std::uint8_t c : *r.warning_error_conditions) {
            w.value(c);
        }
        w.end_array();
    }
    if (r.mode3a_confidence) {
        w.key("I080").begin_object();
        bit_members(w, *r.mode3a_confidence, kQuality12);
        w.end_object();
    }
    if (r.mode_c) {
        const auto& m = *r.mode_c;
        w.key("I100").begin_object();
        flag(w, "V", m.v);
        flag(w, "G", m.g);
        w.key("MODEC").value(m.code);
        bit_members(w, m.low_quality, kQualityModeC);
        w.end_object();
    }
    if (r.height_3d) {
        w.key("I110").begin_object().key("3DH").value(r.height_3d->feet()).end_object();
    }
    if (r.radial_doppler_speed) {
        const auto& s = *r.radial_doppler_speed;
        w.key("I120").begin_object();
        if (s.cal) {
            w.key("CAL").begin_object();
            flag(w, "D", s.cal->d);
            w.key("CAL").value(s.cal->cal);
            w.end_object();
        }
        if (s.rds) {
            w.key("RDS").begin_array();
            for (const auto& x : *s.rds) {
                w.begin_object().key("DOP").value(x.dop).key("AMB").value(x.amb).key("FRQ").value(x.frq).end_object();
            }
            w.end_array();
        }
        w.end_object();
    }
    if (r.comms_acas_capability) {
        const auto& c = *r.comms_acas_capability;
        w.key("I230").begin_object();
        code(w, "COM", c.com);
        code(w, "STAT", c.stat);
        flag(w, "SI", c.si);
        flag(w, "MSSC", c.mssc);
        flag(w, "ARC", c.arc);
        flag(w, "AIC", c.aic);
        flag(w, "B1A", c.b1a);
        w.key("B1B").value(c.b1b);
        w.end_object();
    }
    if (r.acas_resolution_advisory) {
        w.key("I260").value(hex(*r.acas_resolution_advisory, 14));
    }
    if (r.mode1) {
        const auto& m = *r.mode1;
        w.key("I055").begin_object();
        flag(w, "V", m.v);
        flag(w, "G", m.g);
        flag(w, "L", m.l);
        w.key("MODE1").value(m.code);
        w.end_object();
    }
    if (r.mode2) {
        octal_code(w, "I050", "MODE2", *r.mode2);
    }
    if (r.mode1_confidence) {
        w.key("I065").begin_object();
        bit_members(w, *r.mode1_confidence, kQualityMode1);
        w.end_object();
    }
    if (r.mode2_confidence) {
        w.key("I060").begin_object();
        bit_members(w, *r.mode2_confidence, kQuality12);
        w.end_object();
    }
    if (r.special_purpose) {
        w.key("SP").value(hex(*r.special_purpose));
    }
    if (r.reserved_expansion) {
        w.key("RE").value(hex(*r.reserved_expansion));
    }
    w.end_object();
}

void write(Writer& w, const cat034::Record& r) {
    w.begin_object();
    if (r.data_source) {
        source(w, "I010", *r.data_source);
    }
    if (r.message_type) {
        code(w, "I000", *r.message_type);
    }
    if (r.time_of_day) {
        w.key("I030").value(r.time_of_day->seconds());
    }
    if (r.sector_number) {
        w.key("I020").value(r.sector_number->azimuth_deg());
    }
    if (r.antenna_rotation_period) {
        w.key("I041").value(r.antenna_rotation_period->seconds());
    }
    if (r.system_configuration) {
        const auto& s = *r.system_configuration;
        w.key("I050").begin_object();
        if (s.com) {
            w.key("COM").begin_object();
            flag(w, "NOGO", s.com->nogo);
            flag(w, "RDPC", s.com->rdpc);
            flag(w, "RDPR", s.com->rdpr);
            flag(w, "OVLRDP", s.com->ovl_rdp);
            flag(w, "OVLXMT", s.com->ovl_xmt);
            flag(w, "MSC", s.com->msc);
            flag(w, "TSV", s.com->tsv);
            w.end_object();
        }
        if (s.psr) {
            w.key("PSR").begin_object();
            flag(w, "ANT", s.psr->ant);
            w.key("CHAB").value(s.psr->chab);
            flag(w, "OVL", s.psr->ovl);
            flag(w, "MSC", s.psr->msc);
            w.end_object();
        }
        if (s.ssr) {
            w.key("SSR").begin_object();
            flag(w, "ANT", s.ssr->ant);
            w.key("CHAB").value(s.ssr->chab);
            flag(w, "OVL", s.ssr->ovl);
            flag(w, "MSC", s.ssr->msc);
            w.end_object();
        }
        if (s.mds) {
            w.key("MDS").begin_object();
            flag(w, "ANT", s.mds->ant);
            w.key("CHAB").value(s.mds->chab);
            flag(w, "OVLSUR", s.mds->ovl_sur);
            flag(w, "MSC", s.mds->msc);
            flag(w, "SCF", s.mds->scf);
            flag(w, "DLF", s.mds->dlf);
            flag(w, "OVLSCF", s.mds->ovl_scf);
            flag(w, "OVLDLF", s.mds->ovl_dlf);
            w.end_object();
        }
        w.end_object();
    }
    if (r.processing_mode) {
        const auto& m = *r.processing_mode;
        w.key("I060").begin_object();
        if (m.com) {
            w.key("COM").begin_object();
            w.key("REDRDP").value(m.com->red_rdp);
            w.key("REDXMT").value(m.com->red_xmt);
            w.end_object();
        }
        if (m.psr) {
            w.key("PSR").begin_object();
            flag(w, "POL", m.psr->pol);
            w.key("REDRAD").value(m.psr->red_rad);
            w.key("STC").value(m.psr->stc);
            w.end_object();
        }
        if (m.ssr) {
            w.key("SSR").begin_object().key("REDRAD").value(m.ssr->red_rad).end_object();
        }
        if (m.mds) {
            w.key("MDS").begin_object();
            w.key("REDRAD").value(m.mds->red_rad);
            flag(w, "CLU", m.mds->clu);
            w.end_object();
        }
        w.end_object();
    }
    if (r.message_count_values) {
        w.key("I070").begin_array();
        for (const auto& c : *r.message_count_values) {
            w.begin_object().key("TYP").value(c.typ).key("COUNT").value(c.count).end_object();
        }
        w.end_array();
    }
    if (r.polar_window) {
        const auto& p = *r.polar_window;
        w.key("I100").begin_object();
        w.key("RHOST").value(p.rho_start_nm());
        w.key("RHOEND").value(p.rho_end_nm());
        w.key("THETAST").value(p.theta_start_deg());
        w.key("THETAEND").value(p.theta_end_deg());
        w.end_object();
    }
    if (r.data_filter) {
        code(w, "I110", *r.data_filter);
    }
    if (r.position) {
        const auto& p = *r.position;
        w.key("I120").begin_object();
        w.key("HGT").value(p.height);
        w.key("LAT").value(p.latitude_deg());
        w.key("LON").value(p.longitude_deg());
        w.end_object();
    }
    if (r.collimation_error) {
        const auto& c = *r.collimation_error;
        w.key("I090").begin_object().key("RNG").value(c.range_nm()).key("AZM").value(c.azimuth_deg()).end_object();
    }
    if (r.reserved_expansion) {
        w.key("RE").value(hex(*r.reserved_expansion));
    }
    if (r.special_purpose) {
        w.key("SP").value(hex(*r.special_purpose));
    }
    w.end_object();
}

void write(Writer& w, const cat063::Record& r) {
    w.begin_object();
    if (r.data_source) {
        source(w, "I010", *r.data_source);
    }
    if (r.service_identification) {
        w.key("I015").value(*r.service_identification);
    }
    if (r.time_of_message) {
        w.key("I030").value(r.time_of_message->seconds());
    }
    if (r.sensor) {
        source(w, "I050", *r.sensor);
    }
    if (r.sensor_status) {
        const auto& s = *r.sensor_status;
        w.key("I060").begin_object();
        code(w, "CON", s.con);
        flag(w, "PSR", s.psr);
        flag(w, "SSR", s.ssr);
        flag(w, "MDS", s.mds);
        flag(w, "ADS", s.ads);
        flag(w, "MLT", s.mlt);
        if (s.ext1) {
            flag(w, "OPS", s.ext1->ops);
            flag(w, "ODP", s.ext1->odp);
            flag(w, "OXT", s.ext1->oxt);
            flag(w, "MSC", s.ext1->msc);
            flag(w, "TSV", s.ext1->tsv);
            flag(w, "NPW", s.ext1->npw);
        }
        if (s.ext2) {
            populated(w, "TTF", s.ext2->ttf);
            populated(w, "SPO", s.ext2->spo);
        }
        w.end_object();
    }
    if (r.time_stamping_bias_ms) {
        w.key("I070").value(*r.time_stamping_bias_ms);
    }
    if (r.ssr_range_gain_bias) {
        const auto& g = *r.ssr_range_gain_bias;
        w.key("I080").begin_object().key("SRG").value(g.range_gain()).key("SRB").value(g.bias_nm()).end_object();
    }
    if (r.ssr_azimuth_bias) {
        w.key("I081").value(r.ssr_azimuth_bias->degrees());
    }
    if (r.psr_range_gain_bias) {
        const auto& g = *r.psr_range_gain_bias;
        w.key("I090").begin_object().key("PRG").value(g.range_gain()).key("PRB").value(g.bias_nm()).end_object();
    }
    if (r.psr_azimuth_bias) {
        w.key("I091").value(r.psr_azimuth_bias->degrees());
    }
    if (r.psr_elevation_bias) {
        w.key("I092").value(r.psr_elevation_bias->degrees());
    }
    if (r.reserved_expansion) {
        w.key("RE").value(hex(*r.reserved_expansion));
    }
    if (r.special_purpose) {
        w.key("SP").value(hex(*r.special_purpose));
    }
    w.end_object();
}

void write(Writer& w, const Record& record) {
    std::visit([&w](const auto& r) { write(w, r); }, record);
}

void write_members(Writer& w, const DataBlock& block) {
    w.key("category").value(block.category);
    w.key("offset").value(block.offset);
    w.key("length").value(block.length);
    if (!block.supported) {
        w.key("supported").value(false);
    }
    w.key("records").begin_array();
    for (const Record& r : block.records) {
        write(w, r);
    }
    w.end_array();
}

void write(Writer& w, const DataBlock& block) {
    w.begin_object();
    write_members(w, block);
    w.end_object();
}

void write(Writer& w, const DecodeError& error) {
    w.begin_object();
    w.key("code").value(to_string(error.code));
    w.key("offset").value(error.offset);
    w.key("category").value(error.category);
    if (!error.item.empty()) {
        w.key("item").value(error.item);
    }
    w.key("message").value(error.message);
    w.end_object();
}

void write(Writer& w, const DecodeResult& result) {
    w.begin_object();
    w.key("blocks").begin_array();
    for (const DataBlock& b : result.blocks) {
        write(w, b);
    }
    w.end_array();
    w.key("errors").begin_array();
    for (const DecodeError& e : result.errors) {
        write(w, e);
    }
    w.end_array();
    w.end_object();
}

}  // namespace asterix::json

namespace asterix {

std::string to_json(const DecodeResult& result, int indent) {
    std::string out;
    json::Writer w(out, indent);
    json::write(w, result);
    return out;
}

std::string to_json(const DataBlock& block, int indent) {
    std::string out;
    json::Writer w(out, indent);
    json::write(w, block);
    return out;
}

std::string to_json(const Record& record, int indent) {
    std::string out;
    json::Writer w(out, indent);
    json::write(w, record);
    return out;
}

}  // namespace asterix
