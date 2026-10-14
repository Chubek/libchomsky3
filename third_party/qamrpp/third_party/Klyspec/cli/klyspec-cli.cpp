// klyspec-cli -- the bootstrapped CLI for the Klyspec project.
//
// This CLI is itself described by a CLI manifest (cli/CLIManifest.yaml) and is
// built on top of the manifest-driven generation pipeline exposed by
// include/Klyspec.hpp. It can emit C and C++ CLI sources, manpages, and help
// pages from a YAML CLI manifest (parsed via yaml-cpp).

#include "cli-generator.cpp"
#include "help-generator.cpp"
#include "manpage-generator.cpp"

#include <iostream>
#include <string>

namespace {

void print_usage() {
  std::cout <<
      "klyspec - manifest-driven CLI code generator\n"
      "\n"
      "USAGE\n"
      "    klyspec <command> [options]\n"
      "\n"
      "COMMANDS\n"
      "    generate-c <manifest> <output|->          Generate a C CLI source file\n"
      "    generate-cpp <manifest> <output|->        Generate a C++ CLI source file\n"
      "    generate-manpage <manifest> [output|-]    Generate a manpage (ROFF)\n"
      "    generate-help <manifest> [output|-]       Generate a help page (text)\n"
  "    validate <manifest>                       Validate a CLI manifest\n"
  "\n"
  "Manifest format is YAML; a manifest is parsed with yaml-cpp.\n"
  "\n"
  "Use \"-\" (or omit the output argument where optional) to print to stdout.\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }
  const std::string command = argv[1];
  if (command == "-h" || command == "--help" || command == "help") {
    print_usage();
    return 0;
  }
  if (command == "--version") {
    std::cout << "klyspec 1.0.0\n";
    return 0;
  }
  if (command == "generate-c") return klyspec_cli::run_generate_c(argc, argv);
  if (command == "generate-cpp") return klyspec_cli::run_generate_cpp(argc, argv);
  if (command == "generate-manpage") return klyspec_cli::run_generate_manpage(argc, argv);
  if (command == "generate-help") return klyspec_cli::run_generate_help(argc, argv);
  if (command == "validate") return klyspec_cli::run_validate(argc, argv);

  std::cerr << "klyspec: unknown command: " << command << "\n";
  std::cerr << "Run 'klyspec --help' for usage.\n";
  return 2;
}
