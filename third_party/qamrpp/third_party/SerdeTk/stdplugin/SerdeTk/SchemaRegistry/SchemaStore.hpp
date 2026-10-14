#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace serdetk::schema_registry {
struct Schema { std::string identifier; std::string format; std::string content; };
class Store {
public:
    void put(Schema value) { schemas_[value.identifier] = std::move(value); }
    std::optional<Schema> find(const std::string& identifier) const {
        const auto it = schemas_.find(identifier);
        return it == schemas_.end() ? std::nullopt : std::optional<Schema>{it->second};
    }
private:
    std::unordered_map<std::string, Schema> schemas_;
};
} // namespace serdetk::schema_registry
