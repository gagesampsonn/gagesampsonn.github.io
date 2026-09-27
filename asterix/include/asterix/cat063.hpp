#pragma once

// ASTERIX Category 063: Sensor Status Reports.
//
// Item layouts follow EUROCONTROL CAT063 edition 1.7. Editions 1.3 to 1.6 are
// wire compatible subsets (1.7 only adds the second extension of I063/060).
//
// Single-bit flags keep the value transmitted on the wire; the comment on each
// flag gives the meaning of 1. Raw fields keep the transmitted integer and the
// member functions convert them to physical units.

#include "asterix/types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace asterix::cat063 {

/// I063/060 CON: connection status of the sensor.
enum class ConnectionStatus : std::uint8_t {
    Operational = 0,
    Degraded = 1,
    Initialization = 2,
    NotConnected = 3,
};

/// I063/060 Sensor Configuration and Status.
struct SensorStatus {
    ConnectionStatus con = ConnectionStatus::Operational;
    bool psr = false;  ///< 1: PSR NOGO
    bool ssr = false;  ///< 1: SSR NOGO
    bool mds = false;  ///< 1: Mode S NOGO
    bool ads = false;  ///< 1: ADS NOGO
    bool mlt = false;  ///< 1: MLT NOGO

    /// First extension.
    struct Ext1 {
        bool ops = false;  ///< 1: operational use of the system is inhibited
        bool odp = false;  ///< 1: overload in the data processor
        bool oxt = false;  ///< 1: overload in the transmission subsystem
        bool msc = false;  ///< 1: monitoring system disconnected
        bool tsv = false;  ///< 1: time source invalid
        bool npw = false;  ///< 1: no plots being received
    };
    /// Second extension (edition 1.7+).
    struct Ext2 {
        PopulatedValue ttf;  ///< test target failure status: 1 = test target failure
        PopulatedValue spo;  ///< spoofing indication: 1 = potential spoofing attack
    };

    std::optional<Ext1> ext1;
    std::optional<Ext2> ext2;
};

/// I063/080 SSR/Mode S or I063/090 PSR Range Gain and Bias.
/// Corrected range = (measured range - bias) / (1 + gain).
struct RangeGainBias {
    std::int16_t gain = 0;  ///< LSB = 1e-5 (dimensionless)
    std::int16_t bias = 0;  ///< LSB = 1/128 NM

    double range_gain() const noexcept { return gain * 1e-5; }
    double bias_nm() const noexcept { return bias / 128.0; }
};

/// I063/081, I063/091 azimuth bias or I063/092 elevation bias.
struct AngleBias {
    std::int16_t raw = 0;  ///< LSB = 360/2^16 deg

    double degrees() const noexcept { return raw * (360.0 / 65536.0); }
};

/// A decoded CAT063 record. Members are in UAP (FRN) order; a data item that
/// is absent from the record is std::nullopt.
struct Record {
    std::optional<DataSourceId> data_source;                      ///< I063/010, the SDPS sending the report
    std::optional<std::uint8_t> service_identification;           ///< I063/015
    std::optional<TimeOfDay> time_of_message;                     ///< I063/030
    std::optional<DataSourceId> sensor;                           ///< I063/050, the sensor reported on
    std::optional<SensorStatus> sensor_status;                    ///< I063/060
    std::optional<std::int16_t> time_stamping_bias_ms;            ///< I063/070, milliseconds
    std::optional<RangeGainBias> ssr_range_gain_bias;             ///< I063/080
    std::optional<AngleBias> ssr_azimuth_bias;                    ///< I063/081
    std::optional<RangeGainBias> psr_range_gain_bias;             ///< I063/090
    std::optional<AngleBias> psr_azimuth_bias;                    ///< I063/091
    std::optional<AngleBias> psr_elevation_bias;                  ///< I063/092
    std::optional<std::vector<std::uint8_t>> reserved_expansion;  ///< RE, payload without the length octet
    std::optional<std::vector<std::uint8_t>> special_purpose;     ///< SP, payload without the length octet
};

const char* to_string(ConnectionStatus v) noexcept;

}  // namespace asterix::cat063
