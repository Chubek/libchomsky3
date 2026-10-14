// CLI subcommand implementations for the klyspec CLI.
//
// Each generate-* subcommand loads a CLI manifest and emits the corresponding
// artifact (C source, C++ source). The CLI itself is bootstrapped from the
// same manifest capability it exposes to users.

#include "manifest-parser.cpp"

#include <iostream>
#include <string>

namespace klyspec_cli {

auto run_generate_c(int argc, char **argv) -> int {
  if (argc < 4) {
    std::cerr << "usage: klyspec generate-c <manifest> <output|- >\n";
    return 2;
  }
  const auto result = load_manifest(argv[2]);
  if (!result.ok || !result.manifest) {
    for (const auto &diag : result.diagnostics) std::cerr << "klyspec: " << diag << "\n";
    return 1;
  }
  return emit_output(argv[3], klyspec::generate_cli_c(*result.manifest));
}

auto run_generate_cpp(int argc, char **argv) -> int {
  if (argc < 4) {
    std::cerr << "usage: klyspec generate-cpp <manifest> <output|->\n";
    return 2;
  }
  const auto result = load_manifest(argv[2]);
  if (!result.ok || !result.manifest) {
    for (const auto &diag : result.diagnostics) std::cerr << "klyspec: " << diag << "\n";
    return 1;
  }
  return emit_output(argv[3], klyspec::generate_cli_cpp(*result.manifest));
}

auto run_validate(int argc, char **argv) -> int {
  if (argc < 3) {
    std::cerr << "usage: klyspec validate <manifest>\n";
    return 2;
  }
  const auto result = load_manifest(argv[2]);
  if (!result.ok || !result.manifest) {
    for (const auto &diag : result.diagnostics) std::cerr << "klyspec: " << diag << "\n";
    return 1;
  }
  std::cout << "valid: " << result.manifest->program << " " << result.manifest->version
            << " (" << result.manifest->arguments.size() << " arguments, "
            << result.manifest->commands.size() << " commands)\n";
  return 0;
}

} // namespace klyspec_cli
