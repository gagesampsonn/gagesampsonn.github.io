#pragma once

// Human-readable one-line summaries of decoded records, for logs and consoles.

#include "asterix/decoder.hpp"

#include <string>

namespace asterix {

/// One line of text describing the most relevant contents of a record, e.g.
/// "CAT048 25/201 07:35:54.602 ModeS-RC rho=197.684NM theta=340.137deg A=1000 ...".
/// The exact wording is meant for humans and may change; use the typed
/// records or JSON output for machine processing.
std::string summarize(const Record& record);
std::string summarize(const cat034::Record& record);
std::string summarize(const cat048::Record& record);
std::string summarize(const cat063::Record& record);

}  // namespace asterix
