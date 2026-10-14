#include "Klyspec-Subcommand.hpp"

#include <iostream>

class InspectCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.inspect"; }
  std::string name() const override { return "inspect"; }

  int execute(const std::vector<std::string> &args) override {
    bool show_all = false;
    std::string artifact;

    for (const auto &arg : args) {
      if (arg == "--all") {
        show_all = true;
      } else if (!arg.empty() && arg[0] != '-') {
        artifact = arg;
      }
    }

    std::cout << "[inspect] artifact=" << (artifact.empty() ? "(none)" : artifact)
              << " all=" << (show_all ? "yes" : "no") << "\n";
    return artifact.empty() ? 2 : 0;
  }
};

int main() {
  klyspec::SubcommandRegistry registry;
  registry.register_subcommand(std::make_shared<InspectCommand>());
  return registry.dispatch("inspect", {"--all", "build/index.db"});
}
