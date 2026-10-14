/**
 * @file TextPlugin.hpp
 * @brief Example EkippX plugin implementing the text macro pack.
 *
 * The plugin exposes a descriptor plus registration function for static loading.
 * Handlers register through the public PluginAPI and operate on the host-owned
 * Context without taking ownership of runtime state.
 */
#pragma once

#include "EkippX/EkippX-Plugin.hpp"

namespace ekippx::plugins {

inline plugin::StaticPlugin make_text_plugin() {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = "text";
  descriptor.info.display_name = "Text Transform Pack";
  descriptor.info.description = "Text casing and safe-string helpers";
  descriptor.info.author = "EkippX";
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& reg, plugin::PluginContext&) {
        reg.register_function({.name = "reverseText", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          auto text = inv.args.at(0);
          std::reverse(text.begin(), text.end());
          return text;
        });
        reg.register_function({.name = "slugify", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          auto text = detail::lower_copy(inv.args.at(0));
          for (char& ch : text) {
            if (std::isalnum(static_cast<unsigned char>(ch))) continue;
            ch = '-';
          }
          while (text.find("--") != std::string::npos) text = detail::replace_all(std::move(text), "--", "-");
          return detail::trim(text);
        });
        reg.register_function({.name = "shellQuote", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
          std::string text = "'";
          for (char ch : inv.args.at(0)) text += ch == '\'' ? "'\\''" : std::string(1, ch);
          text += "'";
          return text;
        });
      });
}

}  // namespace ekippx::plugins
