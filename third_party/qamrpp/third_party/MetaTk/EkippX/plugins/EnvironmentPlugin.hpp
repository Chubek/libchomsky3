/**
 * @file EnvironmentPlugin.hpp
 * @brief Example EkippX plugin implementing the environment macro pack.
 *
 * The plugin exposes a descriptor plus registration function for static loading.
 * Handlers register through the public PluginAPI and operate on the host-owned
 * Context without taking ownership of runtime state.
 */
#pragma once

#include "EkippX/EkippX-Plugin.hpp"

namespace ekippx::plugins {

inline bool ekippx_plugin_env_name_ok(std::string_view name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; });
}

inline plugin::StaticPlugin make_environment_plugin() {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = "environment";
  descriptor.info.display_name = "Environment Inspection";
  descriptor.info.description = "Environment-backed helpers";
  descriptor.info.author = "EkippX";
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& reg, plugin::PluginContext&) {
        reg.register_function({.name = "hostname", .min_arity = 0, .max_arity = 0}, [](Context&, const Invocation&) {
          if (const char* value = std::getenv("HOSTNAME")) return std::string(value);
          return std::string("unknown-host");
        });
        reg.register_function({.name = "cwd", .min_arity = 0, .max_arity = 0}, [](Context& ctx, const Invocation&) {
          return ctx.config().environment.working_directory.string();
        });
        reg.register_expander({.name = "ENV", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
          if (!ekippx_plugin_env_name_ok(inv.args.at(0))) throw ExpanderError("ENV expects an environment variable name");
          if (const auto found = ctx.config().environment.env_overrides.find(inv.args.at(0)); found != ctx.config().environment.env_overrides.end()) return found->second;
          if (const char* value = std::getenv(inv.args.at(0).c_str())) return std::string(value);
          return std::string{};
        });
      });
}

}  // namespace ekippx::plugins
