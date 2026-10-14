#include "../../ZethaMEM.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        zethamem::Database db;
        zethamem::exec_schema(db, "table users { id: int; name: string; active: bool; }");
        (void)zethamem::parse_query(input);
    } catch (...) {
    }
    return 0;
}
