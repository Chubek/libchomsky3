/**
 * @file FilesystemPlugin.hpp
 * @brief Example EkippX plugin implementing the filesystem macro pack.
 *
 * The plugin exposes a descriptor plus registration function for static loading.
 * Handlers register through the public PluginAPI and operate on the host-owned
 * Context without taking ownership of runtime state.
 */
#pragma once

#include "EkippX/EkippX-Plugin.hpp"

namespace ekippx::plugins {

inline plugin::StaticPlugin make_filesystem_plugin() {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = "filesystem";
  descriptor.info.display_name = "Filesystem Utilities";
  descriptor.info.description = "Path and filesystem helpers";
  descriptor.info.author = "EkippX";
  descriptor.required_capabilities.push_back({"io.filesystem", plugin::CapabilityLevel::required, "filesystem helpers"});
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& reg, plugin::PluginContext&) {
        reg.register_function({.name = "basename", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          if (inv.args.at(0).empty() || detail::path_has_traversal(Path(inv.args.at(0)))) throw FunctionError("basename path must not be empty or contain traversal");
          return Path(inv.args.at(0)).filename().string();
        });
        reg.register_function({.name = "dirname", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          if (inv.args.at(0).empty() || detail::path_has_traversal(Path(inv.args.at(0)))) throw FunctionError("dirname path must not be empty or contain traversal");
          return Path(inv.args.at(0)).parent_path().string();
        });
        reg.register_function({.name = "joinpath", .min_arity = 2, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
          Path result(inv.args.at(0));
          if (result.empty() || detail::path_has_traversal(result)) throw FunctionError("joinpath base path must not be empty or contain traversal");
          for (std::size_t index = 1; index < inv.args.size(); ++index) result /= inv.args.at(index);
          if (detail::path_has_traversal(result)) throw FunctionError("joinpath result must not contain traversal");
          return result.lexically_normal().string();
        });
        reg.register_function({.name = "normalizepath", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          if (inv.args.at(0).empty() || detail::path_has_traversal(Path(inv.args.at(0)))) throw FunctionError("normalizepath path must not be empty or contain traversal");
          return Path(inv.args.at(0)).lexically_normal().string();
        });
      });
}

}  // namespace ekippx::plugins
