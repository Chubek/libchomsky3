#include "BE/Metadata.hpp"

#include <algorithm>
#include <array>

namespace jiterati::be {

namespace {

constexpr std::array<BackendDatasetMetadata, 4> metadata{{
    {"amd64", "amd64", "opcodes/i386-opc.tbl", 1557, 1007},
    {"aarch64", "aarch64", "arch/aarch64-insn.h", 42, 42},
    {"rv64", "riscv", "RISCV-Opdoes", 1072, 1037},
    {"wasm", "wasm", "arch/wasm-binary.h", 670, 623},
}};

} // namespace

BackendDatasetMetadata const* backend_dataset_metadata_begin() { return metadata.data(); }

BackendDatasetMetadata const* backend_dataset_metadata_end() { return metadata.data() + metadata.size(); }

BackendDatasetMetadata const* find_backend_dataset_metadata(std::string_view target) {
    auto it = std::find_if(metadata.begin(), metadata.end(), [&](BackendDatasetMetadata const& item) {
        return item.target == target;
    });
    return it == metadata.end() ? nullptr : &*it;
}

} // namespace jiterati::be
