#pragma once

// ASTERIX Category 048: Monoradar Target Reports.
//
// Item layouts follow EUROCONTROL CAT048 edition 1.32. Editions 1.27 to 1.31
// are wire compatible subsets (they only lack the later extensions of
// I048/020 and some I048/030 codes), so their data decodes identically.
//
// Single-bit flags keep the value transmitted on the wire; the comment on each
// flag gives the meaning of 1. Raw fields keep the transmitted integer and the
// member functions convert them to physical units.

#include "asterix/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asterix::cat048 {

/// I048/020 TYP: type of detection.
enum class DetectionType : std::uint8_t {
    NoDetection = 0,
    SinglePsr = 1,
    SingleSsr = 2,
    SsrPsr = 3,
    SingleModeSAllCall = 4,
    SingleModeSRollCall = 5,
    ModeSAllCallPsr = 6,
    ModeSRollCallPsr = 7,
};

/// I048/020 FOE/FRI: result of a Mode 4 interrogation.
enum class FoeFri : std::uint8_t {
    NoMode4Interrogation = 0,
    FriendlyTarget = 1,
    UnknownTarget = 2,
    NoReply = 3,
};

/// I048/020 Target Report Descriptor.
struct TargetReportDescriptor {
    DetectionType typ = DetectionType::NoDetection;
    bool sim = false;  ///< 1: simulated target report
    bool rdp = false;  ///< 1: report from RDP chain 2
    bool spi = false;  ///< 1: special position identification
    bool rab = false;  ///< 1: report from field monitor (fixed transponder)

    /// First extension.
    struct Ext1 {
        bool tst = false;  ///< 1: test target report
        bool err = false;  ///< 1: extended range present (range in RE ERR)
        bool xpp = false;  ///< 1: X-pulse present
        bool me = false;   ///< 1: military emergency
        bool mi = false;   ///< 1: military identification
        FoeFri foe_fri = FoeFri::NoMode4Interrogation;
    };
    /// Second extension (edition 1.31+).
    struct Ext2 {
        PopulatedValue adsb;  ///< on-site ADS-B information available
        PopulatedValue scn;   ///< surveillance cluster network information available
        PopulatedValue pai;   ///< passive acquisition interface information available
    };
    /// Third extension (edition 1.32+).
    struct Ext3 {
        PopulatedValue acasxv;  ///< ACAS extended version: 0 non-extended, 1 ACAS Xa v1, 2 ACAS Xu v1
        PopulatedValue poxpr;   ///< 1: PO supported by the transponder
    };
    /// Fourth extension (edition 1.32+).
    struct Ext4 {
        PopulatedValue poact;   ///< 1: PO active for the current plot
        PopulatedValue dtfxpr;  ///< 1: basic dataflash supported by the transponder
        PopulatedValue dtfact;  ///< 1: basic dataflash active for the current plot
    };
    /// Fifth extension (edition 1.32+).
    struct Ext5 {
        PopulatedValue irmxpr;  ///< 1: transponder IRM capable
        PopulatedValue irmact;  ///< 1: IRM active for the current plot
    };

    std::optional<Ext1> ext1;
    std::optional<Ext2> ext2;
    std::optional<Ext3> ext3;
    std::optional<Ext4> ext4;
    std::optional<Ext5> ext5;
};

/// I048/040 Measured Position in Polar Co-ordinates.
struct PolarPosition {
    std::uint16_t rho = 0;    ///< slant range, LSB = 1/256 NM
    std::uint16_t theta = 0;  ///< azimuth, LSB = 360/2^16 deg

    double rho_nm() const noexcept { return rho / 256.0; }
    double theta_deg() const noexcept { return theta * (360.0 / 65536.0); }
};

/// I048/042 Calculated Position in Cartesian Co-ordinates.
struct CartesianPosition {
    std::int16_t x = 0;  ///< LSB = 1/128 NM
    std::int16_t y = 0;  ///< LSB = 1/128 NM

    double x_nm() const noexcept { return x / 128.0; }
    double y_nm() const noexcept { return y / 128.0; }
};

/// I048/050 Mode-2 Code or I048/070 Mode-3/A Code in Octal Representation.
struct OctalCode {
    bool v = false;          ///< 1: code not validated
    bool g = false;          ///< 1: garbled code
    bool l = false;          ///< 1: code not extracted during the last scan (smoothed)
    std::uint16_t code = 0;  ///< 12 bits: A4 A2 A1 B4 B2 B1 C4 C2 C1 D4 D2 D1

    /// Four octal digits, e.g. "7700".
    std::string to_string() const { return octal_code(code); }
};

/// I048/055 Mode-1 Code in Octal Representation.
struct Mode1Code {
    bool v = false;         ///< 1: code not validated
    bool g = false;         ///< 1: garbled code
    bool l = false;         ///< 1: smoothed Mode-1 code from a local tracker
    std::uint8_t code = 0;  ///< 5 bits: A4 A2 A1 B2 B1

    /// Two digits: A (0-7) followed by B (0-3), e.g. "73".
    std::string to_string() const;
};

/// I048/090 Flight Level in Binary Representation.
struct FlightLevel {
    bool v = false;          ///< 1: code not validated
    bool g = false;          ///< 1: garbled code
    std::int16_t level = 0;  ///< LSB = 1/4 FL (14-bit two's complement)

    double fl() const noexcept { return level / 4.0; }
    double feet() const noexcept { return level * 25.0; }
};

/// I048/100 Mode-C Code and Code Confidence Indicator.
struct ModeCCode {
    bool v = false;                 ///< 1: code not validated
    bool g = false;                 ///< 1: garbled code
    std::uint16_t code = 0;         ///< reply, 12 bits: C1 A1 C2 A2 C4 A4 B1 D1 B2 D2 B4 D4
    std::uint16_t low_quality = 0;  ///< per-pulse confidence in the same bit order; 1 = low quality

    /// Pressure altitude in feet decoded from `code` (see decode_mode_c_altitude).
    std::optional<std::int32_t> altitude_ft() const noexcept { return decode_mode_c_altitude(code); }
};

/// I048/110 Height Measured by a 3D Radar.
struct Height3D {
    std::int16_t height = 0;  ///< LSB = 25 ft (14-bit two's complement)

    double feet() const noexcept { return height * 25.0; }
};

/// I048/120 Radial Doppler Speed.
struct RadialDopplerSpeed {
    /// Subfield #1: calculated Doppler speed.
    struct Calculated {
        bool d = false;        ///< 1: Doppler speed is doubtful
        std::int16_t cal = 0;  ///< m/s (10-bit two's complement)
    };
    /// Subfield #2 entry: raw Doppler speed.
    struct Raw {
        std::uint16_t dop = 0;  ///< Doppler speed, m/s
        std::uint16_t amb = 0;  ///< ambiguity range, m/s
        std::uint16_t frq = 0;  ///< transmitter frequency, MHz
    };

    std::optional<Calculated> cal;
    std::optional<std::vector<Raw>> rds;
};

/// I048/130 Radar Plot Characteristics. Each subfield is optional.
struct RadarPlotCharacteristics {
    std::optional<std::uint8_t> srl;  ///< SSR plot runlength, LSB = 360/2^13 deg
    std::optional<std::uint8_t> srr;  ///< number of received replies for (M)SSR
    std::optional<std::int8_t> sam;   ///< amplitude of (M)SSR reply, dBm
    std::optional<std::uint8_t> prl;  ///< primary plot runlength, LSB = 360/2^13 deg
    std::optional<std::int8_t> pam;   ///< amplitude of primary plot, dBm
    std::optional<std::int8_t> rpd;   ///< range difference PSR - SSR, LSB = 1/256 NM
    std::optional<std::int8_t> apd;   ///< azimuth difference PSR - SSR, LSB = 360/2^14 deg

    std::optional<double> srl_deg() const noexcept;
    std::optional<double> prl_deg() const noexcept;
    std::optional<double> rpd_nm() const noexcept;
    std::optional<double> apd_deg() const noexcept;
};

/// I048/170 RAD: type of sensor(s) maintaining the track.
enum class TrackSensorType : std::uint8_t {
    Combined = 0,
    Psr = 1,
    SsrModeS = 2,
    Invalid = 3,
};

/// I048/170 CDM: climbing / descending mode.
enum class ClimbDescentMode : std::uint8_t {
    Maintaining = 0,
    Climbing = 1,
    Descending = 2,
    Unknown = 3,
};

/// I048/170 Track Status.
struct TrackStatus {
    bool cnf = false;  ///< 1: tentative track (0: confirmed)
    TrackSensorType rad = TrackSensorType::Combined;
    bool dou = false;  ///< 1: low confidence in plot to track association
    bool mah = false;  ///< 1: horizontal manoeuvre sensed
    ClimbDescentMode cdm = ClimbDescentMode::Maintaining;

    /// First extension.
    struct Ext1 {
        bool tre = false;  ///< 1: end of track lifetime (last report for this track)
        bool gho = false;  ///< 1: ghost target track
        bool sup = false;  ///< 1: maintained with track information from a neighbouring node
        bool tcc = false;  ///< 1: slant range correction and projection to a 2D reference plane
    };
    std::optional<Ext1> ext1;
};

/// I048/200 Calculated Track Velocity in Polar Co-ordinates.
struct PolarVelocity {
    std::uint16_t gsp = 0;  ///< groundspeed, LSB = 2^-14 NM/s
    std::uint16_t hdg = 0;  ///< heading, LSB = 360/2^16 deg

    double groundspeed_nm_s() const noexcept { return gsp / 16384.0; }
    double groundspeed_kt() const noexcept { return gsp * (3600.0 / 16384.0); }
    double heading_deg() const noexcept { return hdg * (360.0 / 65536.0); }
};

/// I048/210 Track Quality (standard deviations).
struct TrackQuality {
    std::uint8_t sigx = 0;  ///< sigma X, LSB = 1/128 NM
    std::uint8_t sigy = 0;  ///< sigma Y, LSB = 1/128 NM
    std::uint8_t sigv = 0;  ///< sigma groundspeed, LSB = 2^-14 NM/s
    std::uint8_t sigh = 0;  ///< sigma heading, LSB = 360/2^12 deg

    double sigma_x_nm() const noexcept { return sigx / 128.0; }
    double sigma_y_nm() const noexcept { return sigy / 128.0; }
    double sigma_v_nm_s() const noexcept { return sigv / 16384.0; }
    double sigma_h_deg() const noexcept { return sigh * (360.0 / 4096.0); }
};

/// I048/230 COM: communications capability of the transponder.
enum class CommCapability : std::uint8_t {
    SurveillanceOnly = 0,
    CommAB = 1,
    CommABUplinkElm = 2,
    CommABUplinkDownlinkElm = 3,
    Level5 = 4,
};

/// I048/230 STAT: flight status.
enum class FlightStatus : std::uint8_t {
    NoAlertNoSpiAirborne = 0,
    NoAlertNoSpiOnGround = 1,
    AlertNoSpiAirborne = 2,
    AlertNoSpiOnGround = 3,
    AlertSpi = 4,
    NoAlertSpi = 5,
    NotAssigned = 6,
    Unknown = 7,
};

/// I048/230 Communications/ACAS Capability and Flight Status.
struct CommsAcasCapability {
    CommCapability com = CommCapability::SurveillanceOnly;
    FlightStatus stat = FlightStatus::NoAlertNoSpiAirborne;
    bool si = false;        ///< 1: II-code capable (0: SI-code capable)
    bool mssc = false;      ///< 1: Mode-S specific service capability
    bool arc = false;       ///< 1: 25 ft altitude reporting (0: 100 ft)
    bool aic = false;       ///< 1: aircraft identification capability
    bool b1a = false;       ///< BDS 1,0 bit 16
    std::uint8_t b1b = 0;   ///< BDS 1,0 bits 37/40
};

/// One entry of I048/250 BDS Register Data.
struct BdsRegister {
    std::uint64_t mb_data = 0;  ///< 56-bit Comm-B message data
    std::uint8_t bds1 = 0;      ///< BDS register address, first digit
    std::uint8_t bds2 = 0;      ///< BDS register address, second digit
};

/// A decoded CAT048 record. Members are in UAP (FRN) order; a data item that
/// is absent from the record is std::nullopt.
struct Record {
    std::optional<DataSourceId> data_source;                         ///< I048/010
    std::optional<TimeOfDay> time_of_day;                            ///< I048/140
    std::optional<TargetReportDescriptor> target_report_descriptor;  ///< I048/020
    std::optional<PolarPosition> measured_position;                  ///< I048/040
    std::optional<OctalCode> mode3a;                                 ///< I048/070
    std::optional<FlightLevel> flight_level;                         ///< I048/090
    std::optional<RadarPlotCharacteristics> radar_plot_characteristics;  ///< I048/130
    std::optional<std::uint32_t> aircraft_address;                   ///< I048/220, 24-bit ICAO address
    std::optional<std::string> aircraft_identification;              ///< I048/240, 8 characters, space padded ('?' marks a code ICAO does not use)
    std::optional<std::vector<BdsRegister>> bds_register_data;       ///< I048/250
    std::optional<std::uint16_t> track_number;                       ///< I048/161, 12 bits
    std::optional<CartesianPosition> calculated_position;            ///< I048/042
    std::optional<PolarVelocity> calculated_velocity;                ///< I048/200
    std::optional<TrackStatus> track_status;                         ///< I048/170
    std::optional<TrackQuality> track_quality;                       ///< I048/210
    std::optional<std::vector<std::uint8_t>> warning_error_conditions;  ///< I048/030, see describe_warning()
    std::optional<std::uint16_t> mode3a_confidence;  ///< I048/080, same bit order as OctalCode::code; 1 = low quality
    std::optional<ModeCCode> mode_c;                                 ///< I048/100
    std::optional<Height3D> height_3d;                               ///< I048/110
    std::optional<RadialDopplerSpeed> radial_doppler_speed;          ///< I048/120
    std::optional<CommsAcasCapability> comms_acas_capability;        ///< I048/230
    std::optional<std::uint64_t> acas_resolution_advisory;           ///< I048/260, 56-bit MB field of BDS 3,0
    std::optional<Mode1Code> mode1;                                  ///< I048/055
    std::optional<OctalCode> mode2;                                  ///< I048/050
    std::optional<std::uint8_t> mode1_confidence;   ///< I048/065, 5 bits QA4 QA2 QA1 QB2 QB1; 1 = low quality
    std::optional<std::uint16_t> mode2_confidence;  ///< I048/060, same bit order as OctalCode::code; 1 = low quality
    std::optional<std::vector<std::uint8_t>> special_purpose;     ///< SP, payload without the length octet
    std::optional<std::vector<std::uint8_t>> reserved_expansion;  ///< RE, payload without the length octet
};

const char* to_string(DetectionType v) noexcept;
const char* to_string(FoeFri v) noexcept;
const char* to_string(TrackSensorType v) noexcept;
const char* to_string(ClimbDescentMode v) noexcept;
const char* to_string(CommCapability v) noexcept;
const char* to_string(FlightStatus v) noexcept;

/// Meaning of an I048/030 warning/error condition code.
const char* describe_warning(std::uint8_t code) noexcept;

}  // namespace asterix::cat048
