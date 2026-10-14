Chapter 1 - Project Model
=========================

PikoRL provides REPL building blocks exposed through ``lpicorl.*`` with a thin public C++
entry point and C feature modules.

Core goals:

- small surface;
- modular feature loading;
- manifest-driven examples;
- build-safe defaults.

Repository structure:

- ``PikoRL.hpp``: public bootstrap and orchestration;
- ``qamrpp-lpikorl/``: C feature wiring and bindings;
- ``examples/``: supported partial feature bundles;
- ``docs/``: Doxygen-driven API and manual pages.
