#include "Klyspec-Plugin.hpp"

#include <cstdlib>
#include <iostream>

class EnvPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "env"; }

  void after_parse(klyspec::ParseResult &parsed) override {
    inject_env(parsed, "KLYSPEC_PROFILE", "profile");
    inject_env(parsed, "KLYSPEC_TARGET", "target");
    inject_env(parsed, "KLYSPEC_JOBS", "jobs");
  }

private:
  static void inject_env(klyspec::ParseResult &parsed, const char *env_name, const char *key) {
    const char *value = std::getenv(env_name);
    if (value != nullptr && !parsed.values.contains(key)) {
      parsed.values[key].push_back(value);
    }
  }
};

int main() {
  EnvPlugin plugin;
  klyspec::ParseResult parsed;
  parsed.values["target"].push_back("cli");

  plugin.after_parse(parsed);
  std::cout << "profile=" << parsed.values["profile"].size() << " ";
  std::cout << "target=" << parsed.values["target"].size() << " ";
  std::cout << "jobs=" << parsed.values["jobs"].size() << "\n";
  return 0;
}
