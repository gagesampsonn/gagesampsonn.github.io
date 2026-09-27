#pragma once

// Data block framing and the top-level decode entry point.

#include "asterix/cat034.hpp"
#include "asterix/cat048.hpp"
#include "asterix/cat063.hpp"
#include "asterix/types.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace asterix {

/// A decoded record of any supported category.
using Record = std::variant<cat034::Record, cat048::Record, cat063::Record>;

/// Category number (34, 48 or 63) of a decoded record.
std::uint8_t category_of(const Record& record) noexcept;

/// One ASTERIX data block: CAT (1 octet), LEN (2 octets), records.
struct DataBlock {
    std::uint8_t category = 0;
    std::size_t offset = 0;       ///< position of the CAT octet in the decoded buffer
    std::size_t length = 0;       ///< LEN: total size of the data block in octets, header included
    bool supported = false;       ///< false for categories this library does not decode
    std::vector<Record> records;  ///< records decoded before the end of the block or the first error
};

/// Result of decoding a buffer that holds zero or more data blocks, e.g. one
/// UDP datagram.
struct DecodeResult {
    std::vector<DataBlock> blocks;
    std::vector<DecodeError> errors;
    std::size_t consumed = 0;  ///< octets covered by complete data blocks

    bool ok() const noexcept { return errors.empty(); }
};

/// True for the categories this library decodes: 34, 48 and 63.
bool is_supported_category(std::uint8_t category) noexcept;

/// Decodes all data blocks in `data[0, size)`.
///
/// Blocks of unsupported categories are returned with `supported == false`
/// and no records. A malformed record ends decoding of its data block (the
/// records before it are kept) and decoding continues with the next block. A
/// broken block header ends decoding; `consumed` then tells how many octets
/// were framed successfully, which lets stream readers keep a partial block.
///
/// Never throws for malformed input (only std::bad_alloc on memory exhaustion)
/// and never reads outside `data[0, size)`.
DecodeResult decode(const std::uint8_t* data, std::size_t size);

inline DecodeResult decode(const std::vector<std::uint8_t>& data) {
    return decode(data.data(), data.size());
}

}  // namespace asterix
