#pragma once

#include "SerdeTk.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace serdetk::plugin {

inline constexpr std::string_view api_version = "1";

class Plugin {
public:
    virtual ~Plugin() = default;
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view version() const noexcept = 0;
    virtual void register_formats(FormatRegistry&) const {}
    [[nodiscard]] virtual std::optional<std::string> resolve_schema(
        std::string_view, std::string_view) const { return std::nullopt; }
    [[nodiscard]] virtual ValidationReport validate(
        const Document&, std::string_view, std::string_view) const { return {}; }
};

class Registry {
public:
    bool register_plugin(std::shared_ptr<Plugin> value) {
        if (!value || value->id().empty() || plugins_.contains(std::string(value->id()))) return false;
        plugins_.emplace(std::string(value->id()), std::move(value));
        return true;
    }

    void register_formats(FormatRegistry& formats = FormatRegistry::instance()) const {
        for (const auto& [_, value] : plugins_) value->register_formats(formats);
    }

    [[nodiscard]] std::shared_ptr<Plugin> find(std::string_view id) const {
        const auto it = plugins_.find(std::string(id));
        return it == plugins_.end() ? nullptr : it->second;
    }

    [[nodiscard]] std::vector<std::string> ids() const {
        std::vector<std::string> out;
        out.reserve(plugins_.size());
        for (const auto& [id, _] : plugins_) out.push_back(id);
        return out;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Plugin>> plugins_;
};

} // namespace serdetk::plugin
