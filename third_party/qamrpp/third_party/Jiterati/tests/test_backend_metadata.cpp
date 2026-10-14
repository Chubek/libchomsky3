#include "BE/Metadata.hpp"

#include <cassert>
#include <string_view>

int main() {
    using jiterati::be::find_backend_dataset_metadata;

    auto amd64 = find_backend_dataset_metadata("amd64");
    assert(amd64 != nullptr);
    assert(amd64->dataset_arch == std::string_view("amd64"));
    assert(amd64->instruction_count == 1557);
    assert(amd64->mnemonic_count == 1007);

    auto aarch64 = find_backend_dataset_metadata("aarch64");
    assert(aarch64 != nullptr);
    assert(aarch64->instruction_count == 42);

    auto rv64 = find_backend_dataset_metadata("rv64");
    assert(rv64 != nullptr);
    assert(rv64->dataset_arch == std::string_view("riscv"));
    assert(rv64->instruction_count == 1072);

    auto wasm = find_backend_dataset_metadata("wasm");
    assert(wasm != nullptr);
    assert(wasm->instruction_count == 670);

    assert(find_backend_dataset_metadata("unknown") == nullptr);
    return 0;
}
