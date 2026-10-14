# Installation

## Requirements

- CMake 3.16 or newer;
- a C++17 compiler;
- a C compiler (required by the project declaration);
- GNU Make, Ninja, or another CMake-supported build tool.

Lua/sol2 support is enabled by default and uses the vendored headers/sources in the repository. Doxygen is optional; without it, the `docs` target reports that documentation generation is unavailable.

## Configure and build

```sh
cmake -S . -B build \
  -DJITERATI_BUILD_TESTS=ON \
  -DJITERATI_BUILD_EXAMPLES=ON
cmake --build build
```

Useful configurations:

```sh
# Core library and CLI only
cmake -S . -B build-min -DJITERATI_BUILD_LUA=OFF -DJITERATI_BUILD_DOCS=OFF

# Enable Lua bindings and tests explicitly
cmake -S . -B build-lua \
  -DJITERATI_BUILD_LUA=ON \
  -DJITERATI_BUILD_TESTS=ON
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

Tests cover core IR, passes, JBL serialization, plugins, backend metadata, Lua bindings, CLI behavior, package safety, and the example IR files when the corresponding options are enabled.

## Install

```sh
cmake --install build --prefix "$HOME/.local"
```

Installation provides:

- static library: `lib/libjiterati.a`;
- CLI: `bin/Jiterati-CLI`;
- public headers: `include/jiterati/`;
- pkg-config metadata: `lib/pkgconfig/jiterati.pc`;
- vendored dependency headers under `include/jiterati/third_party/`;
- generated HTML documentation under `share/doc/jiterati/` when Doxygen is available.

Add the selected prefix to the environment when needed:

```sh
export PATH="$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
```

The CLI help text uses the spelling `jiterati-cli`; the CMake-installed executable is named `Jiterati-CLI`. Create a symlink or shell alias if a lowercase command is preferred.

## Reproducible build directory

The source tree is not modified by the build. To remove generated artifacts, delete only the build directory:

```sh
rm -rf build
```

## Package install root

`Jiterati-CLI package --install` stores packages below `JITERATI_HOME`. The default is `~/.local/jiterati`; override it for isolated installations or CI:

```sh
JITERATI_HOME="$PWD/.jiterati-home" \
  build/Jiterati-CLI package --install Example.jpkg
```

Package installation validates archive paths and requires `META-INF/manifest.json`.
