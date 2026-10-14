#include "Klyspec-Subcommand.hpp"

#include <iostream>

class RunCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.run"; }
  std::string name() const override { return "run"; }
  int execute(const std::vector<std::string> &args) override {
    std::cout << "run target=" << (args.empty() ? "default" : args.front()) << "\n";
    return 0;
  }
};

int main() {
  klyspec::SubcommandRegistry reg;
  reg.register_subcommand(std::make_shared<RunCommand>());
  return reg.dispatch("run", {"app"});
}
