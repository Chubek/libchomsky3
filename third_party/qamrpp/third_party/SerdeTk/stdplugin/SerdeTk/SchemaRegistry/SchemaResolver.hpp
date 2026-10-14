#pragma once

#include "SchemaStore.hpp"

#include <filesystem>

namespace serdetk::schema_registry {
class Resolver {
public:
    explicit Resolver(Store& store) : store_(store) {}
    std::optional<Schema> resolve(const std::string& identifier, const std::filesystem::path& local_path = {});
private:
    Store& store_;
};
} // namespace serdetk::schema_registry
