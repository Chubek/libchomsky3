# Writing Plugins

A current plugin is a C++ extension unit that registers metadata and exposes implementation through normal linkage. Future package/dynamic-loading behavior is governed by `specs/plugin-specs.yaml`.

## Minimal Registration
```cpp
PluginRegistry::instance().register_plugin(
    "example-plugin",
    PluginVersion{0, 1, 0});
```

## Metadata Requirements
A package-distributed plugin should declare:
- stable name;
- semantic version;
- ABI version;
- exported symbols;
- initialization and shutdown hooks;
- required Jiterati version;
- capabilities;
- dependencies.

## Implementation Rules
- Keep semantic IR independent from plugin-specific machine details.
- Validate all user-visible configuration.
- Make initialization idempotent when practical.
- Fail closed on ABI/version mismatch.
- Emit deterministic diagnostics.

## Packaging
Place plugin artifacts under `Plugin/` in a `.jpkg` package and list them in `META-INF/manifest.json`.

## Compatibility
A plugin compiled against one public-header version must not assume private layout stability. Prefer exported C-compatible entry points for future dynamic loading.
