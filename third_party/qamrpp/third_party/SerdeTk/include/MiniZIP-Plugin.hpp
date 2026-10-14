#pragma once

#include "MiniZIP.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minizip::plugin {

inline constexpr std::string_view api_version = "1";

class ArchivePlugin {
public:
    virtual ~ArchivePlugin() = default;
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view version() const noexcept = 0;
    virtual void configure(api::zipper&) const {}
    virtual void configure(api::extractor&) const {}
    [[nodiscard]] virtual api::result<void> verify(
        const std::vector<api::listed_entry>&) const { return {}; }
};

class Registry {
public:
    bool register_plugin(std::shared_ptr<ArchivePlugin> value) {
        if (!value || value->id().empty() || plugins_.contains(std::string(value->id()))) return false;
        plugins_.emplace(std::string(value->id()), std::move(value));
        return true;
    }

    [[nodiscard]] std::shared_ptr<ArchivePlugin> find(std::string_view id) const {
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
    std::unordered_map<std::string, std::shared_ptr<ArchivePlugin>> plugins_;
};

} // namespace minizip::plugin
