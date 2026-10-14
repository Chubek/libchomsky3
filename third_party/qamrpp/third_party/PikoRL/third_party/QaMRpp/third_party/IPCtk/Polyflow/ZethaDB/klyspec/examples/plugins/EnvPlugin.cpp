#include "Klyspec-Plugin.hpp"

#include <cstdlib>
#include <iostream>

class EnvPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "env"; }
  void after_parse(klyspec::ParseResult &parsed) override {
    const char *v = std::getenv("KLYSPEC_PROFILE");
    if (v != nullptr && !parsed.values.contains("profile")) {
      parsed.values["profile"].push_back(v);
    }
  }
};

int main() {
  EnvPlugin p;
  klyspec::ParseResult r;
  p.after_parse(r);
  std::cout << "profile_entries=" << r.values["profile"].size() << "\n";
  return 0;
}
