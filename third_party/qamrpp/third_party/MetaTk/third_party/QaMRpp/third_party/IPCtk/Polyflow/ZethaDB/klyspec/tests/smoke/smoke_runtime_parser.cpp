#include "Klyspec.hpp"

#include <cassert>
#include <vector>

int main() {
  using namespace klyspec;

  Registry registry;
  CommandSpec cmd{};
  cmd.name = "prog";
  assert(registry.register_command(cmd));

  assert(registry.register_argument("prog", ArgumentSpec{.id="a", .kind=ArgumentKind::flag, .value_policy=ValuePolicy::none, .names={"-a", "--all"}}));
  assert(registry.register_argument("prog", ArgumentSpec{.id="b", .kind=ArgumentKind::flag, .value_policy=ValuePolicy::none, .names={"-b"}}));
  assert(registry.register_argument("prog", ArgumentSpec{.id="c", .kind=ArgumentKind::flag, .value_policy=ValuePolicy::none, .names={"-c"}}));
  assert(registry.register_argument("prog", ArgumentSpec{.id="file", .kind=ArgumentKind::option, .value_policy=ValuePolicy::required, .names={"--file"}}));
  assert(registry.register_argument("prog", ArgumentSpec{.id="include", .kind=ArgumentKind::repeatable, .value_policy=ValuePolicy::required, .names={"-I"}}));

  KlyCLIService service(registry);
  auto parsed = service.parse("prog", std::vector<std::string>{"-abc", "--file", "x.txt", "-I", "a", "-I", "b", "--", "--not-opt", "tail"});
  assert(parsed.ok);
  assert(parsed.values.at("a").size() == 1);
  assert(parsed.values.at("b").size() == 1);
  assert(parsed.values.at("c").size() == 1);
  assert(parsed.values.at("file").front() == "x.txt");
  assert(parsed.values.at("include").size() == 2);
  assert(parsed.positionals.size() == 2);

  return 0;
}
