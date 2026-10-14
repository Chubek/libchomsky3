#include "Klyspec.hpp"

#include <iostream>

#define AZMA_CHECK(expr)                                                                    \
  do {                                                                                      \
    if (!(expr)) {                                                                          \
      std::cerr << "[azmatest] check failed: " #expr << " at line " << __LINE__ << "\n"; \
      return 1;                                                                             \
    }                                                                                       \
  } while (0)

int main() {
  klyspec::Registry registry;

  klyspec::OptionSpec alpha;
  alpha.name = "a";
  alpha.kind = klyspec::ArgumentKind::flag;

  klyspec::OptionSpec beta;
  beta.name = "b";
  beta.kind = klyspec::ArgumentKind::flag;

  klyspec::OptionSpec conf;
  conf.name = "config";
  conf.aliases = {"c"};
  conf.kind = klyspec::ArgumentKind::option;

  AZMA_CHECK(registry.register_option(alpha));
  AZMA_CHECK(registry.register_option(beta));
  AZMA_CHECK(registry.register_option(conf));

  std::vector<std::string> argv{"-ab", "--config", "app.yaml", "--", "tail", "args"};
  klyspec::KlyCLIService service;
  auto parsed = service.parse(registry, argv);

  AZMA_CHECK(parsed.ok);
  AZMA_CHECK(parsed.values.contains("a"));
  AZMA_CHECK(parsed.values.contains("b"));
  AZMA_CHECK(parsed.values.contains("config"));
  AZMA_CHECK(parsed.positionals.size() == 2);

  std::cout << "[azmatest] runtime parser unit test passed\n";
  return 0;
}
