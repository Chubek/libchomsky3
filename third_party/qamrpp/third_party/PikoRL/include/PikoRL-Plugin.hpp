#ifndef PIKORL_PLUGIN_HPP
#define PIKORL_PLUGIN_HPP

#include <cstddef>
#include <cstdint>

namespace picorl
{

inline constexpr std::uint32_t plugin_abi_version = 1;

struct PluginHost
{
  std::uint32_t abi_version;
  void *context;
  bool (*register_command) (void *context, const char *name,
                            const char *description);
  void (*report) (void *context, const char *message);
};

struct PluginDescriptor
{
  std::uint32_t abi_version;
  const char *name;
  const char *version;
  const char *description;
};

using PluginInitialize = bool (*) (PluginHost *host,
                                   PluginDescriptor *descriptor);
using PluginShutdown = void (*) (PluginHost *host);

} // namespace picorl

#define PIKORL_PLUGIN_INITIALIZE_SYMBOL "pikorl_plugin_initialize"
#define PIKORL_PLUGIN_SHUTDOWN_SYMBOL "pikorl_plugin_shutdown"

extern "C" bool pikorl_plugin_initialize (
    picorl::PluginHost *host, picorl::PluginDescriptor *descriptor);
extern "C" void pikorl_plugin_shutdown (picorl::PluginHost *host);

#endif
