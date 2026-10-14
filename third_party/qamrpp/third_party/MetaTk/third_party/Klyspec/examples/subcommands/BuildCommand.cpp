#include "Klyspec-Subcommand.hpp"

#include <iostream>

class BuildCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.build"; }
  std::string name() const override { return "build"; }

  int execute(const std::vector<std::string> &args) override {
    bool release = false;
    std::string target = "default";
    for (const auto &arg : args) {
      if (arg == "--release") {
        release = true;
      } else if (arg.rfind("--target=", 0) == 0) {
        target = arg.substr(9);
      }
    }

    std::cout << "[build] target=" << target << " mode=" << (release ? "release" : "debug") << "\n";
    return 0;
  }
};

int main() {
  klyspec::SubcommandRegistry registry;
  registry.register_subcommand(std::make_shared<BuildCommand>());
  return registry.dispatch("build", {"--release", "--target=linux-x86_64"});
}
