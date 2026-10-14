#include "Klyspec-Manifest.hpp"

#include <cassert>

int main() {
  {
    constexpr const char *bad_type = "program:\n  - one\n  - two";
    auto bad_type_result = klyspec::load_cli_manifest(bad_type, "bad-type");
    assert(!bad_type_result.ok);
    assert(!bad_type_result.diagnostics.empty());
  }

  {
    constexpr const char *bad_args_type = R"(
program: klyspec
arguments: {}
)";
    auto bad_args_type_result = klyspec::load_cli_manifest(bad_args_type, "bad-args");
    assert(!bad_args_type_result.ok);
    assert(!bad_args_type_result.diagnostics.empty());
  }

  {
    constexpr const char *dup_arg = R"(
program: klyspec
arguments:
  - id: out
    kind: option
    names: ["-o"]
  - id: out
    kind: flag
    names: ["-v"]
)";
    auto dup_arg_result = klyspec::load_cli_manifest(dup_arg, "dup-arg");
    assert(!dup_arg_result.ok);
    assert(!dup_arg_result.diagnostics.empty());
  }

  {
    constexpr const char *invalid_kind = R"(
program: klyspec
arguments:
  - id: x
    kind: unknown
    names: ["-x"]
)";
    auto bad_kind_result = klyspec::load_cli_manifest(invalid_kind, "bad-kind");
    assert(!bad_kind_result.ok);
    assert(!bad_kind_result.diagnostics.empty());
  }

  {
    constexpr const char *valid = R"(
program: klyspec
version: 1.2.3
about: edges
arguments:
  - id: flag
    kind: flag
    names: ["-f"]
  - id: input
    kind: positional
)";
    auto valid_result = klyspec::load_cli_manifest(valid, "valid");
    assert(valid_result.ok);
    assert(valid_result.manifest->program == "klyspec");
    assert(valid_result.manifest->version == "1.2.3");
  }

  return 0;
}
