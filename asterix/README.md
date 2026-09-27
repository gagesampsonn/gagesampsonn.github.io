# asterix: a C++ decoder for ASTERIX CAT034, CAT048 and CAT063

A dependency-free C++17 library and command-line tool that decode EUROCONTROL
ASTERIX surveillance data:

| Category | Content | Implemented edition | Also decodes |
|---|---|---|---|
| CAT034 | Monoradar service messages: north marker, sector crossing, radar status | 1.29 | 1.27, 1.28 |
| CAT048 | Monoradar target reports: plots and tracks | 1.32 | 1.27 to 1.31 |
| CAT063 | Sensor status reports | 1.7 | 1.3 to 1.6 |

The older editions listed are wire-compatible subsets of the implemented ones.
The only differences are extension octets and code values that older encoders
never send. Earlier CAT048 editions with the same UAP (e.g. 1.21) decode the
same way.

- **Typed records.** Every data item is a `std::optional` member holding the
  transmitted values, with conversions to physical units (`rho_nm()`,
  `theta_deg()`, `fl()`, `groundspeed_kt()`, ...).
- **Complete coverage** of all data items of the three standard UAPs,
  including compound items, all five extensions of I048/020, Mode C Gillham
  altitude decoding and ICAO callsigns.
- **Safe on hostile input.** Every read is bounds-checked. Decoding never
  throws on malformed data and recovers at the next data block. The decoder is
  fuzzed.
- **JSON and text output**, using the EUROCONTROL field mnemonics.
- **`asterix-dump`** decodes pcap, pcapng, raw and hex recordings.
- **Fast:** about 3.7 million records per second on one core.

## How it was verified

- **Unit tests.** 62 tests with hand-computed vectors for every data item of
  all three categories, every error path, pcap/pcapng parsing and JSON output.
- **Differential testing against an independent implementation.**
  [`scripts/crosscheck.py`](scripts/crosscheck.py) builds random but valid
  records from the reference specifications and compares every decoded field
  with [libasterix](https://pypi.org/project/libasterix/), which is generated
  from the machine-readable EUROCONTROL specifications
  ([asterix-specs](https://github.com/zoranbosnjak/asterix-specs)). Result:
  60,000 records covering all 55 data items and 226 subfields, with 0
  mismatches.
- **Real radar traffic.** A recorded capture (100 datagrams, 162 records)
  decodes identically to libasterix. It also matches
  [CroatiaControl's asterix decoder](https://github.com/CroatiaControlLtd/asterix)
  on all 5,492 comparable field values, except I034/120 latitude/longitude.
  There that decoder uses a truncated scale factor (`0.00002145767` instead of
  180/2²³), and this library returns the exact value.
- **Fuzzing.** libFuzzer ran 1.6 million executions, and the in-tree mutation
  tests ran 80,000 cases, all under AddressSanitizer and
  UndefinedBehaviorSanitizer, with no findings.
- **Compilers.** Builds warning-free with GCC 13 and Clang 18 using
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`, as
  C++17 and C++20.

## Building

Requires CMake 3.14+ and a C++17 compiler.

```sh
cmake -S asterix -B build
cmake --build build
./build/asterix-tests              # or: ctest --test-dir build
./build/asterix-dump --help
```

| CMake option | Default | |
|---|---|---|
| `ASTERIX_BUILD_TOOLS` | ON (top level) | `asterix-dump` command line tool |
| `ASTERIX_BUILD_TESTS` | ON (top level) | unit tests |
| `ASTERIX_BUILD_EXAMPLES` | ON (top level) | `examples/decode_datagram.cpp` |
| `ASTERIX_SANITIZE` | OFF | build everything with ASan + UBSan |
| `ASTERIX_BUILD_FUZZER` | OFF | libFuzzer harness (Clang) |
| `ASTERIX_INSTALL` | ON (top level) | install rules and CMake package |

To use it from another CMake project, either vendor the directory:

```cmake
add_subdirectory(asterix)
target_link_libraries(my_app PRIVATE asterix::asterix)
```

or install it (`cmake --install build`) and use `find_package(asterix)`.

## Quick start

```cpp
#include <asterix/asterix.hpp>

void on_datagram(const std::uint8_t* data, std::size_t size) {
    const asterix::DecodeResult result = asterix::decode(data, size);

    for (const asterix::DecodeError& e : result.errors)
        log("ASTERIX error at offset %zu: %s", e.offset, e.message.c_str());

    for (const asterix::DataBlock& block : result.blocks) {
        for (const asterix::Record& record : block.records) {
            if (const auto* plot = std::get_if<asterix::cat048::Record>(&record)) {
                if (plot->measured_position)
                    show(plot->measured_position->rho_nm(), plot->measured_position->theta_deg());
                if (plot->mode3a)
                    label(plot->mode3a->to_string());                 // "7700"
                if (plot->aircraft_identification)
                    label(*plot->aircraft_identification);           // "DLH65A  "
            } else if (const auto* service = std::get_if<asterix::cat034::Record>(&record)) {
                if (service->message_type == asterix::cat034::MessageType::NorthMarker)
                    new_scan();
            } else if (const auto* status = std::get_if<asterix::cat063::Record>(&record)) {
                if (status->sensor_status)
                    sensor_state(status->sensor, status->sensor_status->con);
            }
            std::string json = asterix::to_json(record);             // or asterix::summarize(record)
        }
    }
}
```

A complete program is in [`examples/decode_datagram.cpp`](examples/decode_datagram.cpp).

## API

Headers: `<asterix/asterix.hpp>` includes everything, or include
`decoder.hpp`, `cat034.hpp`, `cat048.hpp`, `cat063.hpp`, `json.hpp` and
`summary.hpp` individually.

### Decoding

```cpp
asterix::DecodeResult asterix::decode(const std::uint8_t* data, std::size_t size);
asterix::DecodeResult asterix::decode(const std::vector<std::uint8_t>& data);

struct DecodeResult {
    std::vector<DataBlock> blocks;
    std::vector<DecodeError> errors;
    std::size_t consumed;            // octets covered by complete data blocks
    bool ok() const;
};

struct DataBlock {
    std::uint8_t category;
    std::size_t offset, length;      // position of the block in the input, LEN field
    bool supported;                  // false: category not decoded, records empty
    std::vector<Record> records;
};

using Record = std::variant<cat034::Record, cat048::Record, cat063::Record>;
std::uint8_t category_of(const Record&);
```

A buffer may hold any number of data blocks (typically one UDP datagram).
Blocks of other categories are framed and returned with `supported == false`,
so mixed feeds work.

### Errors

A malformed record ends decoding of its data block. The records decoded before
it are kept, and decoding resumes at the next block, whose position is known
from LEN. A broken block header ends decoding of the buffer. `consumed` then
says how much was framed, so a stream (TCP) reader can keep the remainder and
retry once more data arrives. Every `DecodeError` carries the code, the byte
offset, the category, the data item (e.g. `"I048/130"`) and a message:

| `ErrorCode` | Meaning |
|---|---|
| `TruncatedBlockHeader` | fewer than 3 octets left for CAT + LEN |
| `InvalidBlockLength` | LEN smaller than 3 |
| `TruncatedBlock` | LEN reaches past the end of the input |
| `TruncatedRecord` | FSPEC or a data item reaches past the end of its block |
| `EmptyRecord` | FSPEC announces no data items |
| `UndefinedItem` | FSPEC flags a spare or undefined FRN (e.g. CAT063 FRN 12) |
| `UndefinedSubfield` | a compound item flags a spare subfield |
| `InvalidExplicitLength` | an RE/SP length octet of zero |

### Record contents

Members are listed in UAP order; an item that is absent is `std::nullopt`.

Conventions used throughout:

- **Raw values plus conversions.** Fields keep the transmitted integer; member
  functions convert to units. For example `PolarPosition::rho` is the raw
  value and `rho_nm()` is nautical miles.
- **Flags.** Single-bit flags keep the wire value, and the header comment gives
  the meaning of 1 (e.g. `OctalCode::v`: 1 = code not validated).
- **Enums.** Coded values are `enum class` types that preserve values the
  specification leaves undefined. `to_string()` gives the specification text.
- **Extensions.** Octets of extended items such as I048/020, I048/170 and
  I063/060 appear as `std::optional<ExtN>` members, present only when
  transmitted.
- **RE and SP** items are returned as their payload bytes.

<details>
<summary><b>CAT048 record</b> (<code>asterix::cat048::Record</code>)</summary>

| FRN | Item | Member | Type |
|---|---|---|---|
| 1 | I048/010 Data Source Identifier | `data_source` | `DataSourceId` |
| 2 | I048/140 Time of Day | `time_of_day` | `TimeOfDay` |
| 3 | I048/020 Target Report Descriptor | `target_report_descriptor` | `TargetReportDescriptor` |
| 4 | I048/040 Measured Position in Polar Co-ordinates | `measured_position` | `PolarPosition` |
| 5 | I048/070 Mode-3/A Code | `mode3a` | `OctalCode` |
| 6 | I048/090 Flight Level | `flight_level` | `FlightLevel` |
| 7 | I048/130 Radar Plot Characteristics | `radar_plot_characteristics` | `RadarPlotCharacteristics` |
| 8 | I048/220 Aircraft Address | `aircraft_address` | `std::uint32_t` |
| 9 | I048/240 Aircraft Identification | `aircraft_identification` | `std::string` |
| 10 | I048/250 BDS Register Data | `bds_register_data` | `std::vector<BdsRegister>` |
| 11 | I048/161 Track Number | `track_number` | `std::uint16_t` |
| 12 | I048/042 Calculated Position in Cartesian Co-ordinates | `calculated_position` | `CartesianPosition` |
| 13 | I048/200 Calculated Track Velocity in Polar Co-ordinates | `calculated_velocity` | `PolarVelocity` |
| 14 | I048/170 Track Status | `track_status` | `TrackStatus` |
| 15 | I048/210 Track Quality | `track_quality` | `TrackQuality` |
| 16 | I048/030 Warning/Error Conditions | `warning_error_conditions` | `std::vector<std::uint8_t>` |
| 17 | I048/080 Mode-3/A Code Confidence Indicator | `mode3a_confidence` | `std::uint16_t` |
| 18 | I048/100 Mode-C Code and Code Confidence Indicator | `mode_c` | `ModeCCode` |
| 19 | I048/110 Height Measured by a 3D Radar | `height_3d` | `Height3D` |
| 20 | I048/120 Radial Doppler Speed | `radial_doppler_speed` | `RadialDopplerSpeed` |
| 21 | I048/230 Communications/ACAS Capability and Flight Status | `comms_acas_capability` | `CommsAcasCapability` |
| 22 | I048/260 ACAS Resolution Advisory Report | `acas_resolution_advisory` | `std::uint64_t` (56 bits) |
| 23 | I048/055 Mode-1 Code | `mode1` | `Mode1Code` |
| 24 | I048/050 Mode-2 Code | `mode2` | `OctalCode` |
| 25 | I048/065 Mode-1 Code Confidence Indicator | `mode1_confidence` | `std::uint8_t` |
| 26 | I048/060 Mode-2 Code Confidence Indicator | `mode2_confidence` | `std::uint16_t` |
| 27 | SP Special Purpose Field | `special_purpose` | `std::vector<std::uint8_t>` |
| 28 | RE Reserved Expansion Field | `reserved_expansion` | `std::vector<std::uint8_t>` |

- `ModeCCode::altitude_ft()` decodes the Gillham-coded reply (100 ft
  steps). When D1 (the Mode S Q bit) is set, it decodes the 25 ft binary
  code instead.
- `aircraft_identification` holds the 8 transmitted characters. Code 0, which
  transponders send when no identification is set, reads as a space; codes
  outside the ICAO character set read as `?`.
- `describe_warning(code)` returns the meaning of an I048/030 code (0 to 37).

</details>

<details>
<summary><b>CAT034 record</b> (<code>asterix::cat034::Record</code>)</summary>

| FRN | Item | Member | Type |
|---|---|---|---|
| 1 | I034/010 Data Source Identifier | `data_source` | `DataSourceId` |
| 2 | I034/000 Message Type | `message_type` | `MessageType` |
| 3 | I034/030 Time of Day | `time_of_day` | `TimeOfDay` |
| 4 | I034/020 Sector Number | `sector_number` | `SectorNumber` |
| 5 | I034/041 Antenna Rotation Period | `antenna_rotation_period` | `AntennaRotationPeriod` |
| 6 | I034/050 System Configuration and Status | `system_configuration` | `SystemConfiguration` (COM, PSR, SSR, MDS) |
| 7 | I034/060 System Processing Mode | `processing_mode` | `ProcessingMode` (COM, PSR, SSR, MDS) |
| 8 | I034/070 Message Count Values | `message_count_values` | `std::vector<MessageCount>` |
| 9 | I034/100 Generic Polar Window | `polar_window` | `PolarWindow` |
| 10 | I034/110 Data Filter | `data_filter` | `DataFilter` |
| 11 | I034/120 3D-Position of Data Source | `position` | `Position3D` |
| 12 | I034/090 Collimation Error | `collimation_error` | `CollimationError` |
| 13 | RE Reserved Expansion Field | `reserved_expansion` | `std::vector<std::uint8_t>` |
| 14 | SP Special Purpose Field | `special_purpose` | `std::vector<std::uint8_t>` |

</details>

<details>
<summary><b>CAT063 record</b> (<code>asterix::cat063::Record</code>)</summary>

| FRN | Item | Member | Type |
|---|---|---|---|
| 1 | I063/010 Data Source Identifier (the SDPS) | `data_source` | `DataSourceId` |
| 2 | I063/015 Service Identification | `service_identification` | `std::uint8_t` |
| 3 | I063/030 Time of Message | `time_of_message` | `TimeOfDay` |
| 4 | I063/050 Sensor Identifier | `sensor` | `DataSourceId` |
| 5 | I063/060 Sensor Configuration and Status | `sensor_status` | `SensorStatus` |
| 6 | I063/070 Time Stamping Bias | `time_stamping_bias_ms` | `std::int16_t` (ms) |
| 7 | I063/080 SSR/Mode S Range Gain and Bias | `ssr_range_gain_bias` | `RangeGainBias` |
| 8 | I063/081 SSR/Mode S Azimuth Bias | `ssr_azimuth_bias` | `AngleBias` |
| 9 | I063/090 PSR Range Gain and Bias | `psr_range_gain_bias` | `RangeGainBias` |
| 10 | I063/091 PSR Azimuth Bias | `psr_azimuth_bias` | `AngleBias` |
| 11 | I063/092 PSR Elevation Bias | `psr_elevation_bias` | `AngleBias` |
| 12 | spare | | |
| 13 | RE Reserved Expansion Field | `reserved_expansion` | `std::vector<std::uint8_t>` |
| 14 | SP Special Purpose Field | `special_purpose` | `std::vector<std::uint8_t>` |

</details>

### JSON and text

```cpp
std::string asterix::to_json(const DecodeResult&, int indent = -1);   // also DataBlock, Record
std::string asterix::summarize(const Record&);                        // one line for logs
asterix::json::Writer                                                 // embed records in your own JSON
```

Records are JSON objects keyed by data item in UAP order. Subfields use the
EUROCONTROL mnemonics, so the output lines up with the specifications and
other tools:

- quantities are in the unit of the specification (NM, degrees, seconds, FL,
  feet);
- flags and codes are integers;
- Mode-2 and Mode-3/A codes are octal strings;
- I048/220, MBDATA, I048/260, RE and SP are upper-case hex strings.

```json
{"I010":{"SAC":25,"SIC":201},"I140":27354.6015625,"I020":{"TYP":5,"SIM":0,"RDP":0,"SPI":0,"RAB":0},
 "I040":{"RHO":197.68359375,"THETA":340.13671875},"I070":{"V":0,"G":0,"L":0,"MODE3A":"1000"},
 "I090":{"V":0,"G":0,"FL":330},"I220":"3C660C","I240":"DLH65A  ",
 "I250":[{"MBDATA":"C0780031BC0000","BDS1":4,"BDS2":0}],"I161":{"TRN":3563},
 "I200":{"GSP":0.12066650390625,"HDG":124.002685546875},
 "I170":{"CNF":0,"RAD":2,"DOU":0,"MAH":0,"CDM":0,"TRE":0,"GHO":0,"SUP":0,"TCC":0},
 "I230":{"COM":1,"STAT":0,"SI":0,"MSSC":1,"ARC":1,"AIC":1,"B1A":1,"B1B":5}}
```

## asterix-dump

```
asterix-dump [OPTIONS] [FILE...]
  -f, --format FMT    input format: auto (default), raw, hex or pcap (pcap and pcapng)
  -x, --hex HEX       decode a hex string instead of files
  -p, --port PORT     captures: only UDP datagrams to or from PORT
  -j, --json          JSON Lines: one object per data block
      --pretty        indented JSON
  -c, --category CAT  only show category CAT (repeatable)
  -q, --quiet         no per-record output
  -s, --stats         statistics on stderr
```

The input format is detected automatically:

- pcap and pcapng captures: Ethernet, VLAN, Linux cooked v1/v2, loopback or
  raw IP link layers, IPv4 or IPv6;
- raw binary recordings of concatenated data blocks;
- text files with one hex-encoded datagram per line.

The exit status is 0 when everything decoded cleanly, 1 when there were decode
errors, and 2 on usage or I/O errors.

```
$ asterix-dump --hex "22 00 10 f6 19 0e 02 3a 69 2b 40 88 40 40 80 00 30 00 30 fd f7 02 19 c9 ..."
CAT034 25/14 08:18:26.336 SectorCrossing sector=90.00deg rdpc=2
CAT048 25/201 07:35:54.602 ModeS-RC rho=197.684NM theta=340.137deg A=1000 FL=330.00 addr=3C660C id=DLH65A trk=3563 gs=434.4kt hdg=124.00deg bds=4,0

$ asterix-dump capture.pcap
#3 2016-05-05T07:35:56.523255Z 10.17.58.184:21154 -> 232.2.1.13:22113 (66 octets)
  CAT048 25/13 07:35:55.859 ModeS-RC rho=194.824NM theta=128.760deg A=2303 FL=360.00 addr=4BAACD ...
  CAT034 25/13 07:35:55.953 SectorCrossing sector=135.00deg

$ asterix-dump --json capture.pcap | jq -c 'select(.category == 48) | .records[] | [.I240, .I090.FL]'
```

## Design

Each category is described by a table of its UAP (`src/cat034.cpp`,
`src/cat048.cpp`, `src/cat063.cpp`). Each entry gives the item's structure
(fixed, extended, repetitive, explicit or compound, with subfield layouts) and
a decode function. A single generic engine (`src/detail.hpp`) runs every
record:

1. It walks the FSPEC.
2. It computes the exact extent of each present item, bounds-checked against
   the data block.
3. It passes only complete items to the decode functions. Their reader also
   returns zeros past the end, so a bug in one decode function cannot read out
   of bounds either.

Adding a category or a new edition means adding a table and decode functions.
The framing and safety logic is shared.

## Limitations and possible extensions

- The contents of the Reserved Expansion Field (e.g. CAT048 Mode 5 and
  extended range reports) and the Special Purpose Field are returned as raw
  bytes.
- I048/250 returns the 56-bit Comm-B data and the register number without
  decoding the register contents (BDS 4,0 selected altitude, 5,0 track and
  turn, 6,0 heading and speed, ...).
- Only the standard UAPs are supported, and only decoding (no encoding).
- `asterix-dump` does not reassemble fragmented IP datagrams (they are counted
  and skipped).

## Cross-checking against the reference implementation

```sh
python3 -m venv venv && venv/bin/pip install libasterix
venv/bin/python scripts/crosscheck.py --dump build/asterix-dump                # 60,000 random records
venv/bin/python scripts/crosscheck.py --dump build/asterix-dump capture.pcap   # recorded traffic
```

## Layout

```
include/asterix/   public headers
src/               decoder implementation (UAP tables in catNNN.cpp)
tools/             asterix-dump and its pcap/pcapng reader
tests/             unit tests (self-contained, no framework needed)
examples/          minimal usage example
fuzz/              libFuzzer harness
scripts/           differential test against libasterix
```
