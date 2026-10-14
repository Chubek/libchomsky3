#include "SchemaRegistry.hpp"

namespace serdetk::schema_registry {
std::optional<std::string> Plugin::resolve_schema(std::string_view identifier, std::string_view) const {
    const auto schema = store_.find(std::string(identifier));
    return schema ? std::optional<std::string>{schema->content} : std::nullopt;
}
} // namespace serdetk::schema_registry
