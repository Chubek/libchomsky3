#include "Klyspec-Subcommand.hpp"

#include <iostream>

class RunCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.run"; }
  std::string name() const override { return "run"; }

  int execute(const std::vector<std::string> &args) override {
    std::string profile = "dev";
    std::string entry = "app";

    for (const auto &arg : args) {
      if (arg.rfind("--profile=", 0) == 0) {
        profile = arg.substr(10);
      } else if (!arg.empty() && arg[0] != '-') {
        entry = arg;
      }
    }

    std::cout << "[run] entry=" << entry << " profile=" << profile << "\n";
    return 0;
  }
};

int main() {
  klyspec::SubcommandRegistry registry;
  registry.register_subcommand(std::make_shared<RunCommand>());
  return registry.dispatch("run", {"--profile=prod", "service-main"});
}
