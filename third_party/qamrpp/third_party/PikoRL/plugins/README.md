# Plugins

Native plugins are shared objects built against `include/PikoRL-Plugin.hpp`.

- export `pikorl_plugin_initialize`;
- reject ABI mismatches before registration;
- export `pikorl_plugin_shutdown` for lifecycle cleanup;
- install shared objects beneath `$PIKORL_HOME/plugins`.
