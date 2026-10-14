#include "../../ZethaMEM.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        zethamem::Database db;
        zethamem::exec_schema(db, "table t { id: int; name: string; }");
        (void)zethamem::exec_query(db, input);
    } catch (...) {
    }
    return 0;
}
