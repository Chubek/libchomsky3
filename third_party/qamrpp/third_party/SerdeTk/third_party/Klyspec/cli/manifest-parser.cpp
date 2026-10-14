#ifndef KLYSPEC_CLI_MANIFEST_PARSER_CPP
#define KLYSPEC_CLI_MANIFEST_PARSER_CPP

// Shared manifest loading helpers for the klyspec CLI.
//
// YAML document is parsed via yaml-cpp; the manifest format is YAML only.
// Loaders surface diagnostics to the caller.

#include "Klyspec-Manifest.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace klyspec_cli {

// Read a whole file into a string. Returns empty and sets failed=true if the
// file cannot be opened.
inline auto read_file(const std::string &path, bool &failed) -> std::string {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    failed = true;
    return {};
  }
  failed = false;
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

// Load a CLI manifest from `path`, printing diagnostics on failure.
inline auto load_manifest(const std::string &path) -> klyspec::CLIManifestResult {
  bool failed = false;
  const auto text = read_file(path, failed);
  if (failed) {
    klyspec::CLIManifestResult result{};
    result.diagnostics.push_back("could not open manifest file: " + path);
    return result;
  }
  return klyspec::load_cli_manifest(text, path);
}

// Write generated text to a file path, or to stdout when path is "-".
inline auto emit_output(const std::string &path, const std::string &content) -> int {
  if (path.empty() || path == "-") {
    std::cout << content;
    return 0;
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    std::cerr << "klyspec: failed to write output: " << path << "\n";
    return 1;
  }
  out << content;
  return 0;
}

} // namespace klyspec_cli

#endif // KLYSPEC_CLI_MANIFEST_PARSER_CPP
