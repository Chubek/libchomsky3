#include "Klyspec-Plugin.hpp"

#include <iostream>

class LoggingPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "logging"; }
  void before_parse(std::vector<std::string> &argv) override {
    std::cout << "[logging] argc=" << argv.size() << "\n";
  }
  void after_dispatch(int code) override {
    std::cout << "[logging] exit=" << code << "\n";
  }
};

int main() {
  LoggingPlugin plugin;
  std::vector<std::string> argv{"--help"};
  plugin.before_parse(argv);
  plugin.after_dispatch(0);
  return 0;
}
