#include "Klyspec-Plugin.hpp"

#include <chrono>
#include <iostream>

class MetricsPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "metrics"; }

  void before_parse(std::vector<std::string> &) override { parse_start_ = std::chrono::steady_clock::now(); }

  void after_parse(klyspec::ParseResult &) override {
    const auto parse_ms = elapsed_ms(parse_start_);
    std::cout << "[metrics] parse_ms=" << parse_ms << "\n";
  }

  void before_dispatch(const klyspec::ParseResult &) override { dispatch_start_ = std::chrono::steady_clock::now(); }

  void after_dispatch(int code) override {
    const auto dispatch_ms = elapsed_ms(dispatch_start_);
    std::cout << "[metrics] dispatch_ms=" << dispatch_ms << " code=" << code << "\n";
  }

private:
  static long long elapsed_ms(const std::chrono::steady_clock::time_point &start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
  }

  std::chrono::steady_clock::time_point parse_start_{};
  std::chrono::steady_clock::time_point dispatch_start_{};
};

int main() {
  MetricsPlugin plugin;
  std::vector<std::string> argv{"run", "app"};
  klyspec::ParseResult parsed;

  plugin.before_parse(argv);
  plugin.after_parse(parsed);
  plugin.before_dispatch(parsed);
  plugin.after_dispatch(0);
  return 0;
}
