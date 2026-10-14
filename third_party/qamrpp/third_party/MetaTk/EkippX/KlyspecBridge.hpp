#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ekippx::cli {

struct CLIParseBridgeResult {
  bool ok{true};
  std::string message{};
  bool show_help{false};
  bool show_version{false};
  bool interactive{false};
  bool check_only{false};
  bool dump_tokens{false};
  bool dump_ast{false};
  bool dump_config{false};
  bool trace{false};
  bool quiet{false};
  bool verbose{false};
  std::optional<std::string> eval_text{};
  std::optional<std::filesystem::path> input_file{};
  std::optional<std::filesystem::path> output_file{};
  std::optional<std::filesystem::path> syntax_file{};
  std::optional<std::filesystem::path> history_file{};
  std::optional<std::filesystem::path> trace_file{};
  std::optional<std::filesystem::path> symtbl_file{};
  std::string trace_format{"json"};
  std::string symtbl_format{"json"};
  std::vector<std::string> include_paths{};
  std::vector<std::string> defines{};
  std::vector<std::string> undefs{};
  std::vector<std::string> config_overrides{};
};

CLIParseBridgeResult parse_cli_with_klyspec(int argc, char** argv);

}  // namespace ekippx::cli
