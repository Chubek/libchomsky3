/**
 * @file RandomPlugin.hpp
 * @brief Example EkippX plugin implementing the random macro pack.
 *
 * The plugin exposes a descriptor plus registration function for static loading.
 * Handlers register through the public PluginAPI and operate on the host-owned
 * Context without taking ownership of runtime state.
 */
#pragma once

#include "EkippX/EkippX-Plugin.hpp"

#include <random>

namespace ekippx::plugins {

inline plugin::StaticPlugin make_random_plugin() {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = "random";
  descriptor.info.display_name = "Random Data";
  descriptor.info.description = "Random text and number helpers";
  descriptor.info.author = "EkippX";
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& reg, plugin::PluginContext&) {
        reg.register_function({.name = "pick", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) -> std::string {
          static thread_local std::mt19937 generator{std::random_device{}()};
          std::uniform_int_distribution<std::size_t> distribution(0, inv.args.size() - 1);
          return inv.args.at(distribution(generator));
        });
        reg.register_function({.name = "randtext", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) -> std::string {
          static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
          static thread_local std::mt19937 generator{std::random_device{}()};
          std::uniform_int_distribution<int> distribution(0, 25);
          std::size_t parsed = 0;
          const auto count = std::stoi(inv.args.at(0), &parsed);
          if (parsed != inv.args.at(0).size() || count < 0 || count > 4096) throw FunctionError("randtext expects length in range [0, 4096]");
          std::string out;
          for (int index = 0; index < count; ++index) out.push_back(alphabet[distribution(generator)]);
          return out;
        });
      });
}

}  // namespace ekippx::plugins
