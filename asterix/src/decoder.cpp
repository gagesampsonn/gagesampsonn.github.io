#include "asterix/decoder.hpp"
#include "detail.hpp"

#include <string>
#include <utility>

namespace asterix {

std::uint8_t category_of(const Record& record) noexcept {
    switch (record.index()) {
        case 0: return 34;
        case 1: return 48;
        default: return 63;
    }
}

bool is_supported_category(std::uint8_t category) noexcept {
    return category == 34 || category == 48 || category == 63;
}

namespace {

DecodeError block_error(ErrorCode code, std::size_t offset, std::uint8_t category, std::string message) {
    DecodeError e;
    e.code = code;
    e.offset = offset;
    e.category = category;
    e.message = std::move(message);
    return e;
}

}  // namespace

DecodeResult decode(const std::uint8_t* data, std::size_t size) {
    DecodeResult result;
    if (data == nullptr) {
        return result;
    }

    std::size_t pos = 0;
    while (pos < size) {
        const std::size_t avail = size - pos;
        if (avail < 3) {
            result.errors.push_back(block_error(ErrorCode::TruncatedBlockHeader, pos, data[pos],
                                                std::to_string(avail) +
                                                    " trailing octet(s) cannot hold a data block header"));
            break;
        }
        const std::uint8_t category = data[pos];
        const std::size_t length = (std::size_t{data[pos + 1]} << 8) | data[pos + 2];
        if (length < 3) {
            result.errors.push_back(block_error(ErrorCode::InvalidBlockLength, pos + 1, category,
                                                "data block LEN is " + std::to_string(length) +
                                                    ", smaller than its own header"));
            break;
        }
        if (length > avail) {
            result.errors.push_back(block_error(ErrorCode::TruncatedBlock, pos + 1, category,
                                                "data block LEN is " + std::to_string(length) + " but only " +
                                                    std::to_string(avail) + " octet(s) are available"));
            break;
        }

        DataBlock block;
        block.category = category;
        block.offset = pos;
        block.length = length;
        block.supported = true;
        const std::size_t begin = pos + 3;
        const std::size_t end = pos + length;
        switch (category) {
            case 34: detail::decode_cat034(data, begin, end, block.records, result.errors); break;
            case 48: detail::decode_cat048(data, begin, end, block.records, result.errors); break;
            case 63: detail::decode_cat063(data, begin, end, block.records, result.errors); break;
            default: block.supported = false; break;
        }
        result.blocks.push_back(std::move(block));
        pos = end;
        result.consumed = pos;
    }
    return result;
}

}  // namespace asterix
