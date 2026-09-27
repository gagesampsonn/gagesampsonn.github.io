#include "asterix/summary.hpp"

#include <cstdarg>
#include <cstdio>

namespace asterix {

namespace {

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void appendf(std::string& out, const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    if (n > 0) {
        out.append(buf, static_cast<std::size_t>(n) < sizeof buf ? static_cast<std::size_t>(n) : sizeof buf - 1);
    }
}

void header(std::string& s, const char* category, const std::optional<DataSourceId>& source,
            const std::optional<TimeOfDay>& time) {
    s += category;
    if (source) {
        appendf(s, " %u/%u", source->sac, source->sic);
    } else {
        s += " -/-";
    }
    if (time) {
        s += ' ';
        s += time->to_string();
    }
}

const char* short_name(cat048::DetectionType t) {
    switch (t) {
        case cat048::DetectionType::NoDetection: return "NoDetection";
        case cat048::DetectionType::SinglePsr: return "PSR";
        case cat048::DetectionType::SingleSsr: return "SSR";
        case cat048::DetectionType::SsrPsr: return "SSR+PSR";
        case cat048::DetectionType::SingleModeSAllCall: return "ModeS-AC";
        case cat048::DetectionType::SingleModeSRollCall: return "ModeS-RC";
        case cat048::DetectionType::ModeSAllCallPsr: return "ModeS-AC+PSR";
        case cat048::DetectionType::ModeSRollCallPsr: return "ModeS-RC+PSR";
    }
    return "?";
}

const char* short_name(cat034::MessageType t) {
    switch (t) {
        case cat034::MessageType::NorthMarker: return "NorthMarker";
        case cat034::MessageType::SectorCrossing: return "SectorCrossing";
        case cat034::MessageType::GeographicalFiltering: return "GeoFiltering";
        case cat034::MessageType::JammingStrobe: return "JammingStrobe";
        case cat034::MessageType::SolarStorm: return "SolarStorm";
        case cat034::MessageType::SsrJammingStrobe: return "SsrJammingStrobe";
        case cat034::MessageType::ModeSJammingStrobe: return "ModeSJammingStrobe";
    }
    return nullptr;
}

std::string trim_right(std::string s) {
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s;
}

void code_flags(std::string& s, bool v, bool g, bool l) {
    if (v) s += "(not-validated)";
    if (g) s += "(garbled)";
    if (l) s += "(smoothed)";
}

}  // namespace

std::string summarize(const cat048::Record& r) {
    std::string s;
    header(s, "CAT048", r.data_source, r.time_of_day);
    if (r.target_report_descriptor) {
        const auto& d = *r.target_report_descriptor;
        s += ' ';
        s += short_name(d.typ);
        if (d.sim) s += " SIMULATED";
        if (d.spi) s += " SPI";
        if (d.rab) s += " FIELD-MONITOR";
        if (d.ext1) {
            if (d.ext1->tst) s += " TEST";
            if (d.ext1->me) s += " MIL-EMERGENCY";
        }
    }
    if (r.measured_position) {
        appendf(s, " rho=%.3fNM theta=%.3fdeg", r.measured_position->rho_nm(), r.measured_position->theta_deg());
    }
    if (r.mode3a) {
        s += " A=" + r.mode3a->to_string();
        code_flags(s, r.mode3a->v, r.mode3a->g, r.mode3a->l);
    }
    if (r.flight_level) {
        appendf(s, " FL=%.2f", r.flight_level->fl());
        code_flags(s, r.flight_level->v, r.flight_level->g, false);
    }
    if (r.mode_c) {
        if (const auto alt = r.mode_c->altitude_ft()) {
            appendf(s, " modeC=%ldft", static_cast<long>(*alt));
        } else {
            s += " modeC=invalid";
        }
    }
    if (r.height_3d) {
        appendf(s, " height3d=%.0fft", r.height_3d->feet());
    }
    if (r.aircraft_address) {
        appendf(s, " addr=%06lX", static_cast<unsigned long>(*r.aircraft_address));
    }
    if (r.aircraft_identification) {
        const std::string id = trim_right(*r.aircraft_identification);
        if (!id.empty()) {
            s += " id=" + id;
        }
    }
    if (r.track_number) {
        appendf(s, " trk=%u", static_cast<unsigned>(*r.track_number));
    }
    if (r.calculated_position) {
        appendf(s, " x=%.3fNM y=%.3fNM", r.calculated_position->x_nm(), r.calculated_position->y_nm());
    }
    if (r.calculated_velocity) {
        appendf(s, " gs=%.1fkt hdg=%.2fdeg", r.calculated_velocity->groundspeed_kt(),
                r.calculated_velocity->heading_deg());
    }
    if (r.track_status) {
        const auto& t = *r.track_status;
        if (t.cnf) s += " tentative";
        if (t.cdm == cat048::ClimbDescentMode::Climbing) s += " climbing";
        if (t.cdm == cat048::ClimbDescentMode::Descending) s += " descending";
        if (t.ext1) {
            if (t.ext1->tre) s += " end-of-track";
            if (t.ext1->gho) s += " ghost";
        }
    }
    if (r.radial_doppler_speed && r.radial_doppler_speed->cal) {
        appendf(s, " doppler=%dm/s", static_cast<int>(r.radial_doppler_speed->cal->cal));
    }
    if (r.mode1) {
        s += " m1=" + r.mode1->to_string();
    }
    if (r.mode2) {
        s += " m2=" + r.mode2->to_string();
    }
    if (r.bds_register_data && !r.bds_register_data->empty()) {
        s += " bds=";
        bool first = true;
        for (const auto& b : *r.bds_register_data) {
            appendf(s, first ? "%u,%u" : "/%u,%u", static_cast<unsigned>(b.bds1), static_cast<unsigned>(b.bds2));
            first = false;
        }
    }
    if (r.warning_error_conditions && !r.warning_error_conditions->empty()) {
        s += " warnings=";
        bool first = true;
        for (const std::uint8_t c : *r.warning_error_conditions) {
            appendf(s, first ? "%u" : ",%u", static_cast<unsigned>(c));
            first = false;
        }
    }
    return s;
}

std::string summarize(const cat034::Record& r) {
    std::string s;
    header(s, "CAT034", r.data_source, r.time_of_day);
    if (r.message_type) {
        if (const char* name = short_name(*r.message_type)) {
            s += ' ';
            s += name;
        } else {
            appendf(s, " type=%u", static_cast<unsigned>(*r.message_type));
        }
    }
    if (r.sector_number) {
        appendf(s, " sector=%.2fdeg", r.sector_number->azimuth_deg());
    }
    if (r.antenna_rotation_period) {
        appendf(s, " rotation=%.3fs", r.antenna_rotation_period->seconds());
    }
    if (r.system_configuration && r.system_configuration->com) {
        const auto& c = *r.system_configuration->com;
        if (c.nogo) s += " NOGO";
        s += c.rdpc ? " rdpc=2" : " rdpc=1";
        if (c.rdpr) s += " RDPC-RESET";
        if (c.ovl_rdp) s += " OVERLOAD-RDP";
        if (c.ovl_xmt) s += " OVERLOAD-XMT";
        if (c.tsv) s += " TIME-INVALID";
    }
    if (r.message_count_values) {
        s += " counts=";
        bool first = true;
        for (const auto& c : *r.message_count_values) {
            appendf(s, first ? "%u:%u" : ",%u:%u", static_cast<unsigned>(c.typ), static_cast<unsigned>(c.count));
            first = false;
        }
    }
    if (r.polar_window) {
        const auto& w = *r.polar_window;
        appendf(s, " window=%.3f-%.3fNM/%.3f-%.3fdeg", w.rho_start_nm(), w.rho_end_nm(), w.theta_start_deg(),
                w.theta_end_deg());
    }
    if (r.data_filter) {
        appendf(s, " filter=%u", static_cast<unsigned>(*r.data_filter));
    }
    if (r.position) {
        appendf(s, " lat=%.6f lon=%.6f height=%dm", r.position->latitude_deg(), r.position->longitude_deg(),
                static_cast<int>(r.position->height));
    }
    if (r.collimation_error) {
        appendf(s, " collimation=%.4fNM/%.4fdeg", r.collimation_error->range_nm(),
                r.collimation_error->azimuth_deg());
    }
    return s;
}

std::string summarize(const cat063::Record& r) {
    std::string s;
    header(s, "CAT063", r.data_source, r.time_of_message);
    if (r.service_identification) {
        appendf(s, " service=%u", static_cast<unsigned>(*r.service_identification));
    }
    if (r.sensor) {
        appendf(s, " sensor=%u/%u", r.sensor->sac, r.sensor->sic);
    }
    if (r.sensor_status) {
        const auto& st = *r.sensor_status;
        s += " con=";
        s += cat063::to_string(st.con);
        std::string nogo;
        if (st.psr) nogo += ",PSR";
        if (st.ssr) nogo += ",SSR";
        if (st.mds) nogo += ",MDS";
        if (st.ads) nogo += ",ADS";
        if (st.mlt) nogo += ",MLT";
        if (!nogo.empty()) {
            s += " NOGO=" + nogo.substr(1);
        }
        if (st.ext1) {
            if (st.ext1->ops) s += " OPS-INHIBITED";
            if (st.ext1->odp) s += " OVERLOAD-DP";
            if (st.ext1->oxt) s += " OVERLOAD-XMT";
            if (st.ext1->msc) s += " MONITORING-DISCONNECTED";
            if (st.ext1->tsv) s += " TIME-INVALID";
            if (st.ext1->npw) s += " NO-PLOTS";
        }
        if (st.ext2) {
            if (st.ext2->ttf.value().value_or(0) != 0) s += " TEST-TARGET-FAILURE";
            if (st.ext2->spo.value().value_or(0) != 0) s += " SPOOFING";
        }
    }
    if (r.time_stamping_bias_ms) {
        appendf(s, " time-bias=%dms", static_cast<int>(*r.time_stamping_bias_ms));
    }
    if (r.ssr_range_gain_bias) {
        appendf(s, " ssr-range-gain=%.5f ssr-range-bias=%.4fNM", r.ssr_range_gain_bias->range_gain(),
                r.ssr_range_gain_bias->bias_nm());
    }
    if (r.ssr_azimuth_bias) {
        appendf(s, " ssr-az-bias=%.4fdeg", r.ssr_azimuth_bias->degrees());
    }
    if (r.psr_range_gain_bias) {
        appendf(s, " psr-range-gain=%.5f psr-range-bias=%.4fNM", r.psr_range_gain_bias->range_gain(),
                r.psr_range_gain_bias->bias_nm());
    }
    if (r.psr_azimuth_bias) {
        appendf(s, " psr-az-bias=%.4fdeg", r.psr_azimuth_bias->degrees());
    }
    if (r.psr_elevation_bias) {
        appendf(s, " psr-el-bias=%.4fdeg", r.psr_elevation_bias->degrees());
    }
    return s;
}

std::string summarize(const Record& record) {
    return std::visit([](const auto& r) { return summarize(r); }, record);
}

}  // namespace asterix
