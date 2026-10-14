#pragma once

#include <cstddef>
#include <string_view>

namespace jiterati::be {

struct BackendDatasetMetadata {
    std::string_view target;
    std::string_view dataset_arch;
    std::string_view dataset_source;
    std::size_t instruction_count;
    std::size_t mnemonic_count;
};

BackendDatasetMetadata const* backend_dataset_metadata_begin();
BackendDatasetMetadata const* backend_dataset_metadata_end();
BackendDatasetMetadata const* find_backend_dataset_metadata(std::string_view target);

} // namespace jiterati::be
