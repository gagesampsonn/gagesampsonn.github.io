#pragma once

// ASTERIX Category 034: Transmission of Monoradar Service Messages.
//
// Item layouts follow EUROCONTROL CAT034 edition 1.29. Editions 1.27 and 1.28
// share the same wire format (1.29 only adds message type and counter values).
//
// Single-bit flags keep the value transmitted on the wire; the comment on each
// flag gives the meaning of 1. Raw fields keep the transmitted integer and the
// member functions convert them to physical units.

#include "asterix/types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace asterix::cat034 {

/// I034/000 Message Type.
enum class MessageType : std::uint8_t {
    NorthMarker = 1,
    SectorCrossing = 2,
    GeographicalFiltering = 3,
    JammingStrobe = 4,
    SolarStorm = 5,
    SsrJammingStrobe = 6,
    ModeSJammingStrobe = 7,
};

/// I034/020 Sector Number: eight most significant bits of the antenna azimuth.
struct SectorNumber {
    std::uint8_t raw = 0;  ///< LSB = 360/2^8 deg

    double azimuth_deg() const noexcept { return raw * (360.0 / 256.0); }
};

/// I034/041 Antenna Rotation Period.
struct AntennaRotationPeriod {
    std::uint16_t raw = 0;  ///< LSB = 1/128 s

    double seconds() const noexcept { return raw / 128.0; }
};

/// I034/050 System Configuration and Status. Each subfield is optional.
struct SystemConfiguration {
    /// Common part (subfield #1).
    struct Common {
        bool nogo = false;     ///< 1: operational use of the system is inhibited
        bool rdpc = false;     ///< 1: radar data processor chain 2 selected (0: chain 1)
        bool rdpr = false;     ///< 1: reset/restart of the selected RDP chain
        bool ovl_rdp = false;  ///< 1: overload in the RDP
        bool ovl_xmt = false;  ///< 1: overload in the transmission subsystem
        bool msc = false;      ///< 1: monitoring system disconnected
        bool tsv = false;      ///< 1: time source invalid
    };
    /// PSR sensor status (subfield #4).
    struct Psr {
        bool ant = false;        ///< 1: antenna 2 selected (0: antenna 1)
        std::uint8_t chab = 0;   ///< 0 no channel, 1 A only, 2 B only, 3 diversity (A and B)
        bool ovl = false;        ///< 1: overload
        bool msc = false;        ///< 1: monitoring system disconnected
    };
    /// SSR sensor status (subfield #5).
    struct Ssr {
        bool ant = false;        ///< 1: antenna 2 selected (0: antenna 1)
        std::uint8_t chab = 0;   ///< 0 no channel, 1 A only, 2 B only, 3 invalid combination
        bool ovl = false;        ///< 1: overload
        bool msc = false;        ///< 1: monitoring system disconnected
    };
    /// Mode S sensor status (subfield #6).
    struct ModeS {
        bool ant = false;        ///< 1: antenna 2 selected (0: antenna 1)
        std::uint8_t chab = 0;   ///< 0 no channel, 1 A only, 2 B only, 3 illegal combination
        bool ovl_sur = false;    ///< 1: surveillance overload
        bool msc = false;        ///< 1: monitoring system disconnected
        bool scf = false;        ///< 1: channel B in use for the surveillance co-ordination function
        bool dlf = false;        ///< 1: channel B in use for the data link function
        bool ovl_scf = false;    ///< 1: overload in the surveillance co-ordination function
        bool ovl_dlf = false;    ///< 1: overload in the data link function
    };

    std::optional<Common> com;
    std::optional<Psr> psr;
    std::optional<Ssr> ssr;
    std::optional<ModeS> mds;
};

/// I034/060 System Processing Mode. Each subfield is optional.
/// Reduction steps are 0 (no reduction active) to 7.
struct ProcessingMode {
    /// Common part (subfield #1).
    struct Common {
        std::uint8_t red_rdp = 0;  ///< reduction step in use for an RDP overload
        std::uint8_t red_xmt = 0;  ///< reduction step in use for a transmission subsystem overload
    };
    /// PSR processing mode (subfield #4).
    struct Psr {
        bool pol = false;          ///< 1: circular polarization (0: linear)
        std::uint8_t red_rad = 0;  ///< reduction step in use for a PSR overload
        std::uint8_t stc = 0;      ///< STC map in use: 0..3 for map 1..4
    };
    /// SSR processing mode (subfield #5).
    struct Ssr {
        std::uint8_t red_rad = 0;  ///< reduction step in use for an SSR overload
    };
    /// Mode S processing mode (subfield #6).
    struct ModeS {
        std::uint8_t red_rad = 0;  ///< reduction step in use for a Mode S overload
        bool clu = false;          ///< 1: cluster state not autonomous
    };

    std::optional<Common> com;
    std::optional<Psr> psr;
    std::optional<Ssr> ssr;
    std::optional<ModeS> mds;
};

/// One entry of I034/070 Message Count Values.
struct MessageCount {
    std::uint8_t typ = 0;     ///< type of message counter (5 bits), see describe_counter()
    std::uint16_t count = 0;  ///< counter value (11 bits)
};

/// I034/100 Generic Polar Window.
struct PolarWindow {
    std::uint16_t rho_start = 0;    ///< LSB = 1/256 NM
    std::uint16_t rho_end = 0;      ///< LSB = 1/256 NM
    std::uint16_t theta_start = 0;  ///< LSB = 360/2^16 deg
    std::uint16_t theta_end = 0;    ///< LSB = 360/2^16 deg

    double rho_start_nm() const noexcept { return rho_start / 256.0; }
    double rho_end_nm() const noexcept { return rho_end / 256.0; }
    double theta_start_deg() const noexcept { return theta_start * (360.0 / 65536.0); }
    double theta_end_deg() const noexcept { return theta_end * (360.0 / 65536.0); }
};

/// I034/110 Data Filter.
enum class DataFilter : std::uint8_t {
    Invalid = 0,
    Weather = 1,
    JammingStrobe = 2,
    Psr = 3,
    SsrModeS = 4,
    SsrModeSPsr = 5,
    EnhancedSurveillance = 6,
    PsrEnhancedSurveillance = 7,
    PsrEnhancedSurveillanceSsrOutsideApi = 8,
    PsrEnhancedSurveillanceAllSsr = 9,
};

/// I034/120 3D-Position of Data Source (WGS-84).
struct Position3D {
    std::int16_t height = 0;  ///< height above the WGS-84 ellipsoid, metres
    std::int32_t lat = 0;     ///< 24-bit two's complement, LSB = 180/2^23 deg
    std::int32_t lon = 0;     ///< 24-bit two's complement, LSB = 180/2^23 deg

    double latitude_deg() const noexcept { return lat * (180.0 / 8388608.0); }
    double longitude_deg() const noexcept { return lon * (180.0 / 8388608.0); }
};

/// I034/090 Collimation Error.
struct CollimationError {
    std::int8_t range = 0;    ///< LSB = 1/128 NM
    std::int8_t azimuth = 0;  ///< LSB = 360/2^14 deg

    double range_nm() const noexcept { return range / 128.0; }
    double azimuth_deg() const noexcept { return azimuth * (360.0 / 16384.0); }
};

/// A decoded CAT034 record. Members are in UAP (FRN) order; a data item that
/// is absent from the record is std::nullopt.
struct Record {
    std::optional<DataSourceId> data_source;                        ///< I034/010
    std::optional<MessageType> message_type;                        ///< I034/000
    std::optional<TimeOfDay> time_of_day;                           ///< I034/030
    std::optional<SectorNumber> sector_number;                      ///< I034/020
    std::optional<AntennaRotationPeriod> antenna_rotation_period;   ///< I034/041
    std::optional<SystemConfiguration> system_configuration;        ///< I034/050
    std::optional<ProcessingMode> processing_mode;                  ///< I034/060
    std::optional<std::vector<MessageCount>> message_count_values;  ///< I034/070
    std::optional<PolarWindow> polar_window;                        ///< I034/100
    std::optional<DataFilter> data_filter;                          ///< I034/110
    std::optional<Position3D> position;                             ///< I034/120
    std::optional<CollimationError> collimation_error;              ///< I034/090
    std::optional<std::vector<std::uint8_t>> reserved_expansion;    ///< RE, payload without the length octet
    std::optional<std::vector<std::uint8_t>> special_purpose;       ///< SP, payload without the length octet
};

const char* to_string(MessageType v) noexcept;
const char* to_string(DataFilter v) noexcept;

/// Meaning of an I034/070 message counter type.
const char* describe_counter(std::uint8_t typ) noexcept;

}  // namespace asterix::cat034
