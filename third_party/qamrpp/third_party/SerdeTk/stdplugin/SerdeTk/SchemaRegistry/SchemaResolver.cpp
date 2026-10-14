#include "SchemaResolver.hpp"

#include <fstream>
#include <sstream>

namespace serdetk::schema_registry {
std::optional<Schema> Resolver::resolve(const std::string& identifier, const std::filesystem::path& local_path) {
    if (const auto cached = store_.find(identifier)) return cached;
    if (local_path.empty()) return std::nullopt;
    std::ifstream in(local_path);
    if (!in) return std::nullopt;
    std::ostringstream content;
    content << in.rdbuf();
    Schema value{identifier, {}, content.str()};
    store_.put(value);
    return value;
}
} // namespace serdetk::schema_registry
