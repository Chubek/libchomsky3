Chapter 14 - Native Plugins
===========================

Plugins include ``PikoRL-Plugin.hpp`` and export
``pikorl_plugin_initialize`` and ``pikorl_plugin_shutdown``. Initialization
receives a versioned ``PluginHost`` and returns a ``PluginDescriptor``.

Reject ABI-version mismatches before registration. Plugin code must use host
callbacks only; core implementation objects are not ABI. Install shared
objects under ``$PIKORL_HOME/plugins``.
