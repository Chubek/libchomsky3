Chapter 9 - Platform Architecture
=================================

PikoRL separates five artifact classes:

- core runtime: public API and REPL host;
- extensions: structured runtime capabilities described by ``manifests/Extensions.yaml``;
- addons: installable user packages under ``$PIKORL_HOME/addons``;
- plugins: native shared objects using ``PikoRL-Plugin.hpp``;
- interop scripts: code evaluated by hosted QaMRpp or S7 runtimes.

Interop scripts are neither addons nor plugins. Standard extension assets are
versioned in ``stdext/`` and install under ``$PIKORL_HOME/stdlib``.
