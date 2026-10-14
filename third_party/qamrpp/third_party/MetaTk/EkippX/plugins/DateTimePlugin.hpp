/**
 * @file DateTimePlugin.hpp
 * @brief Example EkippX plugin implementing the datetime macro pack.
 *
 * The plugin exposes a descriptor plus registration function for static loading.
 * Handlers register through the public PluginAPI and operate on the host-owned
 * Context without taking ownership of runtime state.
 */
#pragma once

#include "EkippX/EkippX-Plugin.hpp"

namespace ekippx::plugins {

inline plugin::StaticPlugin make_datetime_plugin() {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = "datetime";
  descriptor.info.display_name = "Date and Time";
  descriptor.info.description = "Extra date formatting helpers";
  descriptor.info.author = "EkippX";
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& reg, plugin::PluginContext&) {
        reg.register_function({.name = "datefmt", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          if (inv.args.at(0).size() > 64) throw FunctionError("datefmt format string is too long");
          if (inv.args.at(0).find_first_of("\r\n") != std::string::npos) throw FunctionError("datefmt format must be single-line");
          const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
          std::tm tm{};
#if defined(_WIN32)
          localtime_s(&tm, &now);
#else
          localtime_r(&now, &tm);
#endif
          std::ostringstream stream;
          stream << std::put_time(&tm, inv.args.at(0).c_str());
          return stream.str();
        });
        reg.register_expander({.name = "YEAR", .min_arity = 0, .max_arity = 0}, [](Context&, const Invocation&) {
          const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
          std::tm tm{};
#if defined(_WIN32)
          localtime_s(&tm, &now);
#else
          localtime_r(&now, &tm);
#endif
          return std::to_string(1900 + tm.tm_year);
        });
      });
}

}  // namespace ekippx::plugins
