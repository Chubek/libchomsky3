#include "Klyspec-Plugin.hpp"

#include <iostream>

class LoggingPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "logging"; }

  void before_parse(std::vector<std::string> &argv) override {
    std::cout << "[logging] entering parse, argv=" << argv.size() << "\n";
  }

  void after_parse(klyspec::ParseResult &parsed) override {
    std::cout << "[logging] parsed keys=" << parsed.values.size() << "\n";
  }

  void before_dispatch(const klyspec::ParseResult &parsed) override {
    std::cout << "[logging] dispatch with " << parsed.values.size() << " value buckets\n";
  }

  void after_dispatch(int code) override {
    std::cout << "[logging] dispatch exit=" << code << "\n";
  }
};

int main() {
  LoggingPlugin plugin;
  std::vector<std::string> argv{"build", "--release", "--verbose"};
  klyspec::ParseResult result;
  result.values["release"].push_back("true");
  result.values["verbose"].push_back("true");

  plugin.before_parse(argv);
  plugin.after_parse(result);
  plugin.before_dispatch(result);
  plugin.after_dispatch(0);
  return 0;
}
