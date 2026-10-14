#include "Klyspec-Plugin.hpp"

#include <cassert>
#include <memory>

class CountingPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "count"; }
  void before_parse(std::vector<std::string> &) override { ++before_parse_calls; }
  void after_parse(klyspec::ParseResult &) override { ++after_parse_calls; }
  void before_dispatch(const klyspec::ParseResult &) override { ++before_dispatch_calls; }
  void after_dispatch(int) override { ++after_dispatch_calls; }

  int before_parse_calls{0};
  int after_parse_calls{0};
  int before_dispatch_calls{0};
  int after_dispatch_calls{0};
};

int main() {
  auto plugin = std::make_shared<CountingPlugin>();
  klyspec::PluginRegistry plugins;
  assert(plugins.register_plugin(plugin));

  std::vector<std::string> argv{"--help"};
  klyspec::ParseResult parsed;

  plugins.before_parse(argv);
  plugins.after_parse(parsed);
  plugins.before_dispatch(parsed);
  plugins.after_dispatch(0);

  assert(plugin->before_parse_calls == 1);
  assert(plugin->after_parse_calls == 1);
  assert(plugin->before_dispatch_calls == 1);
  assert(plugin->after_dispatch_calls == 1);
  return 0;
}
