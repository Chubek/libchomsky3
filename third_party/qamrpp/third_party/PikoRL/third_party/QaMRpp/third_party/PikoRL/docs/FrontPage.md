PikoRL Documentation
====================

PikoRL is a small, header-oriented REPL toolkit built around QaMRpp and the ``lpicorl.*`` feature surface.

## Manual

- [Chapter 1 - Project Model](manual/Chapter-1.md)
- [Chapter 2 - Runtime and Data Roles](manual/Chapter-2.md)
- [Chapter 3 - Public API](manual/Chapter-3.md)
- [Chapter 4 - Feature Modules](manual/Chapter-4.md)
- [Chapter 5 - Example Bundles and Manifest](manual/Chapter-5.md)
- [Chapter 6 - Build Integration](manual/Chapter-6.md)
- [Chapter 7 - Documentation Pipeline](manual/Chapter-7.md)
- [Chapter 8 - Operational Notes](manual/Chapter-8.md)

## Architecture

- Public entry point: ``PikoRL.hpp``.
- Feature implementation: ``qamrpp-lpikorl/``.
- Runtime/script integration: QaMRpp.
- Structured data parsing: SerdeTk.

## Feature Surface

The ``lpicorl`` namespace includes prompt, syntax, directives, completion, help, history,
plugins, page, menu, web, sandboxing, IDL, theme, colors, and macros.

## Build + Docs

- Root CMake option: ``PICORL_BUILD_DOCS`` (default: ``ON``).
- Docs target: ``docs``.
- Doxygen config source: ``docs/Doxyfile.in``.
