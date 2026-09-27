#pragma once

// Umbrella header for the ASTERIX decoder library.
//
//   #include <asterix/asterix.hpp>
//
//   asterix::DecodeResult result = asterix::decode(datagram, size);
//   for (const asterix::DataBlock& block : result.blocks)
//       for (const asterix::Record& record : block.records)
//           if (const auto* plot = std::get_if<asterix::cat048::Record>(&record))
//               if (plot->measured_position)
//                   use(plot->measured_position->rho_nm(), plot->measured_position->theta_deg());

#include "asterix/cat034.hpp"
#include "asterix/cat048.hpp"
#include "asterix/cat063.hpp"
#include "asterix/decoder.hpp"
#include "asterix/json.hpp"
#include "asterix/summary.hpp"
#include "asterix/types.hpp"
