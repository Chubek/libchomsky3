Chapter 18 - Installation Layout
=================================

``PIKORL_HOME`` is the runtime root:

- ``$PIKORL_HOME/bin/pikorl``: primary executable;
- ``$PIKORL_HOME/addons``: packaged user addons;
- ``$PIKORL_HOME/plugins``: native plugins;
- ``$PIKORL_HOME/stdlib``: standard library and standard extensions.

Build-time install prefix configuration must map to this layout. Do not install
runtime assets into examples, source paths, or system-global directories.
