Chapter 8 - Profiles
=====================

Profiles are loaded with ``KlyProfileLoader`` from ``Klyspec-Profiles.hpp``.

Backends are delegated to SerdeTk builtins:

- JSON,
- XML,
- YAML,
- S-Expr.

Profile values are currently extracted from object-like roots as string/int values.
