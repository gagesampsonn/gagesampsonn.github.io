// libFuzzer harness. Build with -DASTERIX_BUILD_FUZZER=ON using Clang, then run
//   ./asterix-fuzz -max_total_time=300 corpus/

#include <asterix/asterix.hpp>

#include <cstddef>
#include <cstdint>

static volatile std::size_t g_sink;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const asterix::DecodeResult result = asterix::decode(data, size);
    std::size_t sink = asterix::to_json(result).size();
    for (const asterix::DataBlock& block : result.blocks) {
        if (block.offset + block.length > size) {
            __builtin_trap();
        }
        for (const asterix::Record& record : block.records) {
            sink += asterix::summarize(record).size();
        }
    }
    if (result.consumed > size) {
        __builtin_trap();
    }
    g_sink = sink;
    return 0;
}
