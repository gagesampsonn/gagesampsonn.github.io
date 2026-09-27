// Minimal example: decode one UDP payload and use the typed records.
//
// In a real application `datagram` is what recvfrom() returned on the
// radar's (usually multicast) ASTERIX port.

#include <asterix/asterix.hpp>

#include <cstdint>
#include <cstdio>

int main() {
    // One CAT034 sector crossing message followed by one CAT048 target report.
    const std::uint8_t datagram[] = {
        0x22, 0x00, 0x10, 0xf6, 0x19, 0x0e, 0x02, 0x3a, 0x69, 0x2b, 0x40, 0x88, 0x40, 0x40, 0x80, 0x00,
        0x30, 0x00, 0x30, 0xfd, 0xf7, 0x02, 0x19, 0xc9, 0x35, 0x6d, 0x4d, 0xa0, 0xc5, 0xaf, 0xf1, 0xe0,
        0x02, 0x00, 0x05, 0x28, 0x3c, 0x66, 0x0c, 0x10, 0xc2, 0x36, 0xd4, 0x18, 0x20, 0x01, 0xc0, 0x78,
        0x00, 0x31, 0xbc, 0x00, 0x00, 0x40, 0x0d, 0xeb, 0x07, 0xb9, 0x58, 0x2e, 0x41, 0x00, 0x20, 0xf5};

    const asterix::DecodeResult result = asterix::decode(datagram, sizeof datagram);

    for (const asterix::DecodeError& error : result.errors) {
        std::fprintf(stderr, "decode error at offset %zu (%s): %s\n", error.offset, asterix::to_string(error.code),
                     error.message.c_str());
    }

    for (const asterix::DataBlock& block : result.blocks) {
        for (const asterix::Record& record : block.records) {
            if (const auto* plot = std::get_if<asterix::cat048::Record>(&record)) {
                std::printf("CAT048 target report\n");
                if (plot->data_source) {
                    std::printf("  radar         SAC %u / SIC %u\n", plot->data_source->sac, plot->data_source->sic);
                }
                if (plot->time_of_day) {
                    std::printf("  time          %s UTC\n", plot->time_of_day->to_string().c_str());
                }
                if (plot->measured_position) {
                    std::printf("  position      %.3f NM at %.3f deg\n", plot->measured_position->rho_nm(),
                                plot->measured_position->theta_deg());
                }
                if (plot->mode3a) {
                    std::printf("  squawk        %s\n", plot->mode3a->to_string().c_str());
                }
                if (plot->flight_level) {
                    std::printf("  flight level  %.2f\n", plot->flight_level->fl());
                }
                if (plot->aircraft_address) {
                    std::printf("  ICAO address  %06X\n", static_cast<unsigned>(*plot->aircraft_address));
                }
                if (plot->aircraft_identification) {
                    std::printf("  callsign      '%s'\n", plot->aircraft_identification->c_str());
                }
                if (plot->calculated_velocity) {
                    std::printf("  ground speed  %.1f kt, heading %.1f deg\n",
                                plot->calculated_velocity->groundspeed_kt(), plot->calculated_velocity->heading_deg());
                }
            } else if (const auto* service = std::get_if<asterix::cat034::Record>(&record)) {
                std::printf("CAT034 service message: %s\n",
                            service->message_type ? asterix::cat034::to_string(*service->message_type) : "?");
                if (service->sector_number) {
                    std::printf("  antenna azimuth %.2f deg\n", service->sector_number->azimuth_deg());
                }
            } else if (const auto* status = std::get_if<asterix::cat063::Record>(&record)) {
                if (status->sensor_status) {
                    std::printf("CAT063 sensor status: %s\n", asterix::cat063::to_string(status->sensor_status->con));
                }
            }

            // Any record can also be serialised to JSON.
            std::printf("  json: %s\n", asterix::to_json(record).c_str());
        }
    }
    return result.ok() ? 0 : 1;
}
