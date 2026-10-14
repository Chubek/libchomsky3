# QaMRpp Installation Guide

```bash
cmake -S . -B build
cmake --build build -j4
cmake --install build
```

The default user-local root is `~/.qamrpp`. Set either `QAMRPP_HOME` or its
alias `QAMRPP_PREFIX_DIR` at configure time to change it:

```bash
cmake -DQAMRPP_PREFIX_DIR=$HOME/.local/qamrpp -S . -B build
```

Installed assets use these locations:

- CLI tools: `${QAMRPP_HOME}/bin`;
- standard-library shared objects: `${QAMRPP_HOME}/lib/stdlua`;
- standard plugins: `${QAMRPP_HOME}/plugins`;
- C/C++ qlib headers: `${CMAKE_INSTALL_INCLUDEDIR}/qamrpp`;
- podlets: `${QAMRPP_HOME}/podlet`.

Runtime lookup honors `QAMRPP_HOME`, `QAMRPP_PREFIX_DIR`, and `QAMRPP_PATH`.

Optional components are enabled by default:

```bash
cmake -DBUILD_STDLIB=OFF -DBUILD_STANDARD_PLUGINS=OFF -DINSTALL_QLIB=OFF \
  -DMAKE_DOCS=OFF -S . -B build
```

Use `INSTALL_CLI_TO_SYSTEM_BIN=ON` for system CLI binaries. Documentation is
controlled by `MAKE_DOCS=ON|OFF`.
