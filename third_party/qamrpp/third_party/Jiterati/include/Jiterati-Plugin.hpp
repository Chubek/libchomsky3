/** @file Jiterati-Plugin.hpp
 *  @brief Plugin system declarations (placeholder).
 */
#ifndef JITERATI_PLUGIN_HPP_INCLUDED
#define JITERATI_PLUGIN_HPP_INCLUDED

#include <string>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace jiterati {

struct PluginVersion {
    uint32_t major = 0;
    uint32_t minor = 1;
    uint32_t patch = 0;
};

class PluginRegistry {
public:
    static PluginRegistry& instance();
    void register_plugin(const char* name, PluginVersion v);
    std::size_t count() const;
    bool has_plugin(std::string const& name) const;
    PluginVersion version_of(std::string const& name) const;
private:
    PluginRegistry() = default;
    std::unordered_map<std::string, PluginVersion> plugins_;
};

inline PluginRegistry& PluginRegistry::instance() {
    static PluginRegistry r;
    return r;
}

inline void PluginRegistry::register_plugin(const char* name, PluginVersion version) {
    if (name == nullptr || *name == '\0') return;
    plugins_[name] = version;
}
inline std::size_t PluginRegistry::count() const { return plugins_.size(); }
inline bool PluginRegistry::has_plugin(std::string const& name) const {
    return plugins_.find(name) != plugins_.end();
}
inline PluginVersion PluginRegistry::version_of(std::string const& name) const {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) throw std::out_of_range("plugin not registered: " + name);
    return it->second;
}

} // namespace jiterati

#endif // JITERATI_PLUGIN_HPP_INCLUDED
