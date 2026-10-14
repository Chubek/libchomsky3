Chapter 13 - Embedded S7 Scheme
===============================

S7 integration belongs in ``interop/S7Scheme``. Bind only explicit host
operations and preserve host ownership of runtime state, registration tables,
and lifecycle transitions.

Scheme evaluation must report source diagnostics through the host boundary.
Scheme scripts remain embedded-runtime artifacts; they are not native plugins
and must not be installed as addons without an addon package definition.
