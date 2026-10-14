#pragma once

#include "SchemaResolver.hpp"
#include "SerdeTk-Plugin.hpp"

namespace serdetk::schema_registry {
class Plugin final : public serdetk::plugin::Plugin {
public:
    std::string_view id() const noexcept override { return "serdetk.schema-registry"; }
    std::string_view version() const noexcept override { return "1.0.0"; }
    std::optional<std::string> resolve_schema(std::string_view identifier, std::string_view) const override;
    Store& store() noexcept { return store_; }
private:
    mutable Store store_;
};
} // namespace serdetk::schema_registry
