Chapter 1 - Overview
====================

Klyspec is a header-only C++20 CLI specification framework. It provides:

- native and textual DSL entry points,
- command/argument registry,
- parser runtime,
- plugin hooks,
- native subcommand dispatch,
- IPC adapters,
- multi-format profile loading.

The code is organized around small composable headers:

- ``Klyspec.hpp`` for the core model, registry, runtime parser, and Klytmk parser,
- ``Klyspec-Plugin.hpp`` for plugin contracts,
- ``Klyspec-Subcommand.hpp`` for native subcommands,
- ``Klyspec-IPC.hpp`` for IPC bridge scaffolding over IPCtk,
- ``Klyspec-Profiles.hpp`` for SerdeTk-based profile parsing.
