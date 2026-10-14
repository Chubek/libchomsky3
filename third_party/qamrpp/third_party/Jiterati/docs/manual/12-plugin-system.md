# Plugin System

The current public plugin API is a lightweight registry. The full plugin ABI is specified in `specs/plugin-specs.yaml`.

## Implemented API
`include/Jiterati-Plugin.hpp` defines:
```cpp
struct PluginVersion { int major; int minor; int patch; };
class PluginRegistry {
public:
    static PluginRegistry& instance();
    void register_plugin(const char* name, PluginVersion version);
    std::size_t count() const;
    bool has_plugin(std::string const& name) const;
    PluginVersion version_of(std::string const& name) const;
};
```

## Bundled Plugins
`Plugins/` contains examples:
- `BURSISel.cpp`
- `MaxMunchISel.cpp`
- `ListISched.cpp`

## Specified ABI Metadata
`specs/plugin-specs.yaml` covers:
- name/version/author;
- ABI version;
- exported symbols;
- initialization/shutdown;
- dependencies;
- capabilities;
- extension points.

## Registry Semantics
- Registration is by unique string name.
- Version lookup returns registered version metadata.
- Missing plugins must be handled explicitly by callers.

## CLI Surface
```sh
jiterati-cli plugin --list
```

## Boundary
Do not assume dynamic loading, symbol negotiation, or capability validation unless implemented in the CLI/package loader or specified for future work.
