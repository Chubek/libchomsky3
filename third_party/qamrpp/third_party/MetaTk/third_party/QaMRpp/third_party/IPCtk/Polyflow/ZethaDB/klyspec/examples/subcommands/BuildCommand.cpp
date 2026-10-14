#include "Klyspec-Subcommand.hpp"

#include <iostream>

class BuildCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.build"; }
  std::string name() const override { return "build"; }
  int execute(const std::vector<std::string> &args) override {
    std::cout << "build args=" << args.size() << "\n";
    return 0;
  }
};

int main() {
  klyspec::SubcommandRegistry reg;
  reg.register_subcommand(std::make_shared<BuildCommand>());
  return reg.dispatch("build", {"--release"});
}
