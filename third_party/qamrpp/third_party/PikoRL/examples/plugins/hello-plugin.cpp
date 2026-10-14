#include <PikoRL-Plugin.hpp>

extern "C" bool
pikorl_plugin_initialize (picorl::PluginHost *host,
                          picorl::PluginDescriptor *descriptor)
{
  if (host == nullptr || descriptor == nullptr
      || host->abi_version != picorl::plugin_abi_version)
    return false;

  *descriptor = {
    picorl::plugin_abi_version,
    "hello-plugin",
    "0.1.0",
    "Minimal PikoRL plugin example"
  };
  return host->register_command != nullptr
             && host->register_command (host->context, "hello",
                                        "Minimal plugin command");
}

extern "C" void
pikorl_plugin_shutdown (picorl::PluginHost *)
{
}
