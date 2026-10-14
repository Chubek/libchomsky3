# QaMRpp Installation Guide

QaMRpp now supports configurable install destinations for CLI tools, stdlibs, and stdlib++ headers while preserving the default local-per-user layout.

## Quick Start

Build and install with defaults (user-local layout):

```bash
cmake -S . -B build
cmake --build build -j4
cmake --install build
```

By default:

- `QAMRPP_HOME` is `~/.qamrpp`
- stdlib shared objects install to `${QAMRPP_HOME}`
- stdlib++ headers install to `${QAMRPP_HOME}/stdc++`
- plugin headers install to `${QAMRPP_HOME}/plugins`
- `podpack --install` defaults to `${QAMRPP_HOME}/podlet`
- CLI tools default to `${QAMRPP_HOME}/bin`
- Documentation outputs under `${CMAKE_INSTALL_DOCDIR}/qamrpp`.

## Overriding `QAMRPP_HOME`

You can change the local install root at configure time:

```bash
cmake -DQAMRPP_HOME=~/.local/qamrpp -S . -B build
```

All local paths that are not explicitly overridden by a system-lib/binary toggle use the value of `QAMRPP_HOME`.

## Build docs

Docs are enabled by default:

- `MAKE_DOCS=ON`: generate HTML and LaTeX docs in one configure/build pass.
- `MAKE_DOCS=OFF`: skip docs generation entirely.

```bash
cmake -S . -B build -DMAKE_DOCS=ON
cmake --build build --target docs
cmake --install build
```

Documentation front page is `docs/FrontPage.md` and is populated with links to every manual in `docs/manual`.

## Install to system directories (optional)

### CLI binaries

Install CLI tools to CMake’s system binary dir (`CMAKE_INSTALL_BINDIR`):

```bash
cmake -DINSTALL_CLI_TO_SYSTEM_BIN=ON -S . -B build
cmake --build build
cmake --install build
```

Without this flag, they install to `${QAMRPP_HOME}/bin` by default.

### stdlib shared objects

Install stdlib libraries to system lib directory (`CMAKE_INSTALL_LIBDIR`):

```bash
cmake -DINSTALL_STDLIB_TO_SYSTEM_LIB=ON -S . -B build
cmake --build build
cmake --install build
```

Without this flag, they install to `${QAMRPP_HOME}`.

### stdlib++ headers

Install stdlib++ headers to `${CMAKE_INSTALL_LIBDIR}/qamrpp/stdc++`:

```bash
cmake -DINSTALL_STDLIBPP_TO_SYSTEM_LIB=ON -S . -B build
cmake --build build
cmake --install build
```

Without this flag, they install to `${QAMRPP_HOME}/stdc++`.

## Common combinations

- Local all-in-one layout (default behavior):

  ```bash
  cmake -DQAMRPP_HOME=~/.local/qamrpp -S . -B build
  cmake --build build -j4
  cmake --install build
  ```

- System install for CLI, local stdlibs:

  ```bash
  cmake -DQAMRPP_HOME=~/.local/qamrpp -DINSTALL_CLI_TO_SYSTEM_BIN=ON -S . -B build
  cmake --build build -j4
  cmake --install build
  ```

- Full system install for CLI + stdlib + stdlib++:

  ```bash
  cmake -DINSTALL_CLI_TO_SYSTEM_BIN=ON -DINSTALL_STDLIB_TO_SYSTEM_LIB=ON -DINSTALL_STDLIBPP_TO_SYSTEM_LIB=ON -S . -B build
  cmake --build build -j4
  cmake --install build
  ```

- Docs with custom destination:

  ```bash
  cmake -DCMAKE_INSTALL_DOCDIR=share/doc/qamrpp-docs -S . -B build
  cmake --build build -j4
  cmake --install build
  ```

## Notes

- If you use a custom `QAMRPP_HOME`, keep `podpack`, runtime loader, and `qamrpp-cli` consistent by exporting it in your environment:
  - `export QAMRPP_HOME=~/.local/qamrpp` (or your chosen path)
- `QAMRPP_HOME` is used by runtime lookup, plugin install, stdlib loading defaults, and `podpack --install` default destination.
