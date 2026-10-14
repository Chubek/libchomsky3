#include "Klyspec.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  using namespace klyspec;

  Registry registry;
  CommandSpec build_cmd{};
  build_cmd.name = "build";
  build_cmd.help = "Build project artifacts";
  assert(registry.register_command(build_cmd));
  assert(registry.register_alias("build", "b"));

  ArgumentSpec verbose{};
  verbose.id = "verbose";
  verbose.kind = ArgumentKind::flag;
  verbose.value_policy = ValuePolicy::none;
  verbose.names = {"-v", "--verbose"};
  verbose.help = "Enable verbose output";
  assert(registry.register_argument("build", verbose));

  ArgumentSpec file{};
  file.id = "file";
  file.kind = ArgumentKind::option;
  file.value_policy = ValuePolicy::required;
  file.names = {"-f", "--file"};
  file.help = "Input file";
  file.required = true;
  assert(registry.register_argument("b", file));

  const auto *cmd = registry.lookup("b");
  assert(cmd != nullptr);
  assert(cmd->name == "build");

  const auto *arg = registry.lookup_argument("build", "--file");
  assert(arg != nullptr);
  assert(arg->id == "file");

  KlyCLIService service(registry);
  const ParseResult parsed = service.parse("build", std::vector<std::string>{"-v", "--file", "in.txt", "target"});
  assert(parsed.ok);
  assert(parsed.values.contains("verbose"));
  assert(parsed.values.contains("file"));
  assert(parsed.values.at("file").front() == "in.txt");
  assert(parsed.positionals.size() == 1);

  return 0;
}
