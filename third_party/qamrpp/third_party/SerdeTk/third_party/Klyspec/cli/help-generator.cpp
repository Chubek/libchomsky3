// Help-page generation subcommand for the klyspec CLI.

#include "manifest-parser.cpp"

#include <iostream>
#include <string>

namespace klyspec_cli {

auto run_generate_help(int argc, char **argv) -> int {
  if (argc < 3) {
    std::cerr << "usage: klyspec generate-help <manifest> [output|-]\n";
    return 2;
  }
  const auto result = load_manifest(argv[2]);
  if (!result.ok || !result.manifest) {
    for (const auto &diag : result.diagnostics) std::cerr << "klyspec: " << diag << "\n";
    return 1;
  }
  const auto text = klyspec::generate_help_page(*result.manifest);
  return emit_output(argc >= 4 ? argv[3] : "-", text);
}

} // namespace klyspec_cli
