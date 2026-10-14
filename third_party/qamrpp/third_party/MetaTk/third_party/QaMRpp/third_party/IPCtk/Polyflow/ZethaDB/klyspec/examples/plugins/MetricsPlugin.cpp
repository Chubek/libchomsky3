#include "Klyspec-Plugin.hpp"

#include <chrono>
#include <iostream>

class MetricsPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "metrics"; }
  void before_dispatch(const klyspec::ParseResult &) override { start_ = std::chrono::steady_clock::now(); }
  void after_dispatch(int) override {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();
    std::cout << "[metrics] dispatch_ms=" << ms << "\n";
  }
private:
  std::chrono::steady_clock::time_point start_{};
};

int main() {
  MetricsPlugin p;
  p.before_dispatch(klyspec::ParseResult{});
  p.after_dispatch(0);
  return 0;
}
