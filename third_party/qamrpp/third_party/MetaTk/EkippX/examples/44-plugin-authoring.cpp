#include "EkippX/EkippX-Plugin.hpp"

#include <iostream>

int main() {
  auto ctx = ekippx::batteries::batteries_context();
  ekippx::plugin::PluginManager manager(ctx);
  manager.register_static(ekippx::plugin::make_static_plugin(
      {.info = {.name = "demo-author", .display_name = "Demo Author", .description = "Adds helloAuthor", .author = "EkippX"}},
      [](ekippx::plugin::Registrar& reg, ekippx::plugin::PluginContext&) {
        reg.register_function({.name = "helloAuthor", .min_arity = 1, .max_arity = 1}, [](ekippx::Context&, const ekippx::Invocation& inv) {
          return std::string("hello ") + inv.args.at(0);
        });
      }));
  ctx.activate_plugin("demo-author");
  std::cout << ctx.expand_text("@emitln(&helloAuthor(world))");
}
