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

  klyspec::OptionSpec opt;
  opt.name = "file";
  opt.aliases = {"f"};
  opt.help = "Input file";
  opt.required = true;
  opt.default_value = "default.txt";

  AZMA_CHECK(registry.register_option(opt));

  const auto *found = registry.find_option("file");
  AZMA_CHECK(found != nullptr);
  AZMA_CHECK(found->required);
  AZMA_CHECK(found->aliases.size() == 1);

  const auto *alias = registry.find_option("f");
  AZMA_CHECK(alias != nullptr);
  AZMA_CHECK(alias->name == "file");

  std::cout << "[azmatest] registry unit test passed\n";
  return 0;
}
