# Chapter-2: Build and Installation

## CMake Workflow

- out-of-source: `cmake -S . -B build`.
- build: `cmake --build build -j`.
- install: `cmake --install build --prefix <prefix>`.

## Options

- `IPCTK_BUILD_EXAMPLES`: build C++ examples.
- `IPCTK_BUILD_TESTS`: build unit/fuzz entry targets.
- `IPCTK_BUILD_DOCS`: enable Doxygen target.

## Install Outputs

- headers to `${CMAKE_INSTALL_INCLUDEDIR}`.
- destination templates to `${CMAKE_INSTALL_DATADIR}/ipctk/dest`.
- `ipctk.pc` to `${CMAKE_INSTALL_LIBDIR}/pkgconfig`.

## Documentation

- Doxygen config source: `docs/Doxyfile.in`.
- target: `cmake --build build --target docs`.

## Troubleshooting

- link errors in examples usually indicate missing `main` or stale source state.
- include resolution issues indicate missing `-I` or non-installed header paths.
