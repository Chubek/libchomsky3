#include "../../ZethaMEM.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        (void)zethamem::parse_schema(input);
    } catch (...) {
    }
    return 0;
}
