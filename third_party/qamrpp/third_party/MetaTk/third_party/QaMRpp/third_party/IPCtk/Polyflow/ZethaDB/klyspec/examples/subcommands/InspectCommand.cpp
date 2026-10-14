#include "Klyspec-Subcommand.hpp"

#include <iostream>

class InspectCommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "native.inspect"; }
  std::string name() const override { return "inspect"; }
  int execute(const std::vector<std::string> &args) override {
    std::cout << "inspect argc=" << args.size() << "\n";
    return 0;
  }
};

int main() {
  klyspec::SubcommandRegistry reg;
  reg.register_subcommand(std::make_shared<InspectCommand>());
  return reg.dispatch("inspect", {"--all", "obj"});
}
