# Klyspec Usage

This document shows practical usage of the current Klyspec API.

## 1) Core Registry + Runtime Parser

```cpp
#include "Klyspec.hpp"

#include <iostream>

int main() {
  using namespace klyspec;

  Registry registry;
  registry.register_command(CommandSpec{.name = "prog", .help = "Sample program"});

  registry.register_argument("prog", ArgumentSpec{
      .id = "a",
      .kind = ArgumentKind::flag,
      .value_policy = ValuePolicy::none,
      .names = {"-a", "--all"}
  });

  registry.register_argument("prog", ArgumentSpec{
      .id = "file",
      .kind = ArgumentKind::option,
      .value_policy = ValuePolicy::required,
      .names = {"--file"},
      .required = true
  });

  KlyCLIService cli(registry);
  auto parsed = cli.parse("prog", {"-a", "--file", "input.txt", "tail"});

  if (!parsed.ok) {
    for (const auto &d : parsed.diagnostics) std::cerr << d << "\n";
    return 1;
  }

  std::cout << "file=" << parsed.values["file"].front() << "\n";
  std::cout << "positionals=" << parsed.positionals.size() << "\n";
  return 0;
}
```

Supported parse behaviors:

- short options (`-a`)
- long options (`--file`)
- stacked flags (`-abc`)
- repeated options
- `--` end-of-options sentinel
- positional capture

## 2) Parse Klytmk DSL

```cpp
#include "Klyspec.hpp"

int main() {
  const char* source = R"(
command "build"
{
};
)";

  auto out = klyspec::parse_klytmk(source);
  if (!out.ok) return 1;
  return out.ast->nodes.empty() ? 1 : 0;
}
```

Diagnostics include:

- line
- column
- message

## 3) Plugins

```cpp
#include "Klyspec-Plugin.hpp"

#include <memory>

class LoggingPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "logging"; }
  void before_parse(std::vector<std::string>&) override {}
  void after_dispatch(int) override {}
};

int main() {
  auto plugin = std::make_shared<LoggingPlugin>();
  klyspec::PluginRegistry reg;
  reg.register_plugin(plugin);

  std::vector<std::string> argv{"--help"};
  klyspec::ParseResult parsed;

  reg.before_parse(argv);
  reg.after_parse(parsed);
  reg.before_dispatch(parsed);
  reg.after_dispatch(0);
  return 0;
}
```

Hook order:

1. `before_parse`
2. `after_parse`
3. `before_dispatch`
4. `after_dispatch`

## 4) Native Subcommands

```cpp
#include "Klyspec-Subcommand.hpp"

#include <memory>

class BuildCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.build"; }
  std::string name() const override { return "build"; }
  int execute(const std::vector<std::string>& args) override {
    return args.empty() ? 1 : 0;
  }
};

int main() {
  klyspec::SubcommandRegistry registry;
  registry.register_subcommand(std::make_shared<BuildCommand>());
  return registry.dispatch("build", {"--release"});
}
```

## 5) IPC Bridge (IPCtk)

```cpp
#include "Klyspec-IPC.hpp"

int main() {
  klyspec::ParseResult parsed;
  parsed.ok = true;
  parsed.values["file"].push_back("x.txt");

  klyspec::KlyIPCService ipc;
  ipc.enable<klyspec::IPC::Signal>([](const klyspec::ParseResult &r) {
    return r.ok ? 0 : 1;
  });

  auto payload = ipc.serialize(parsed);
  if (!payload.is_ok()) return 1;

  auto rc = ipc.dispatch(parsed);
  return (rc && *rc == 0) ? 0 : 1;
}
```

## 6) Profiles (SerdeTk)

```cpp
#include "Klyspec-Profiles.hpp"

int main() {
  auto loaded = klyspec::KlyProfileLoader::load(
      klyspec::KlyProfileLoader::Format::json,
      R"({"file":"build.json","profile":"release"})");

  if (!loaded) return 1;
  return loaded->contains("file") ? 0 : 1;
}
```

Supported formats:

- JSON
- XML
- YAML
- S-Expr

## 7) Compile Examples

```bash
g++ -std=c++20 -Wall -Wextra -pedantic examples/plugins/LoggingPlugin.cpp -I. -o /tmp/kly_log
g++ -std=c++20 -Wall -Wextra -pedantic examples/subcommands/BuildCommand.cpp -I. -o /tmp/kly_build
```

## 8) Notes

- Keep all library code in headers.
- Prefer incremental compile/smoke loops.
- See `tests/smoke/` and `manual/` for more details.
