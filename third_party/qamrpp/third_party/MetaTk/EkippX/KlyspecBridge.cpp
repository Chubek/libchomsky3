/**
 * @file KlyspecBridge.cpp
 * @brief Small adapter between EkippX CLI option structures and Klyspec argument parsing.
 *
 * The bridge centralizes command-line vocabulary so the CLI, tests, and manual
 * describe one option model. Parsing errors are surfaced as structured values for
 * the caller to render.
 */
#include "KlyspecBridge.hpp"

#include "Klyspec.hpp"

namespace ekippx::cli {

CLIParseBridgeResult parse_cli_with_klyspec(int argc, char** argv) {
  using namespace klyspec;

  Registry registry;
  registry.register_command(CommandSpec{.name = "ekippx", .help = "EkippX CLI"});
  const auto add_flag = [&](std::string id, std::vector<std::string> names) {
    registry.register_argument("ekippx", ArgumentSpec{.id = std::move(id), .kind = ArgumentKind::flag, .value_policy = ValuePolicy::none, .names = std::move(names)});
  };
  const auto add_option = [&](std::string id, std::vector<std::string> names) {
    registry.register_argument("ekippx", ArgumentSpec{.id = std::move(id), .kind = ArgumentKind::option, .value_policy = ValuePolicy::required, .names = std::move(names)});
  };

  add_flag("help", {"-h", "--help"});
  add_flag("version", {"-V", "--version"});
  add_flag("interactive", {"-i", "--interactive"});
  add_flag("stdin", {"--stdin"});
  add_flag("check", {"--check"});
  add_flag("dump_tokens", {"--dump-tokens"});
  add_flag("dump_ast", {"--dump-ast"});
  add_flag("dump_config", {"--dump-config"});
  add_flag("trace", {"--trace"});
  add_flag("quiet", {"-q", "--quiet"});
  add_flag("verbose", {"-v", "--verbose"});
  add_option("eval", {"-e", "--eval"});
  add_option("output", {"-o", "--output"});
  add_option("include_path", {"-I", "--include-path"});
  add_option("define", {"-D", "--define"});
  add_option("undef", {"-U", "--undef"});
  add_option("config", {"-c", "--config"});
  add_option("repl_syntax", {"--repl-syntax"});
  add_option("repl_history", {"--repl-history"});
  add_option("trace_file", {"--trace-file"});
  add_option("trace_format", {"--trace-format"});
  add_option("dump_symtbl", {"--dump-symtbl"});
  add_option("symtbl_format", {"--symtbl-format"});

  KlyCLIService service(registry);
  std::vector<std::string> args;
  for (int index = 1; index < argc; ++index) args.emplace_back(argv[index]);
  const auto parsed = service.parse("ekippx", args);

  CLIParseBridgeResult result;
  result.ok = parsed.ok;
  if (!parsed.ok) {
    for (std::size_t index = 0; index < parsed.diagnostics.size(); ++index) {
      if (index != 0) result.message += "\n";
      result.message += parsed.diagnostics[index];
    }
  }
  result.show_help = parsed.values.contains("help");
  result.show_version = parsed.values.contains("version");
  result.interactive = parsed.values.contains("interactive");
  result.check_only = parsed.values.contains("check");
  result.dump_tokens = parsed.values.contains("dump_tokens");
  result.dump_ast = parsed.values.contains("dump_ast");
  result.dump_config = parsed.values.contains("dump_config");
  result.trace = parsed.values.contains("trace");
  result.quiet = parsed.values.contains("quiet");
  result.verbose = parsed.values.contains("verbose");
  if (parsed.values.contains("eval")) result.eval_text = parsed.values.at("eval").back();
  if (parsed.values.contains("output")) result.output_file = parsed.values.at("output").back();
  if (parsed.values.contains("include_path")) result.include_paths = parsed.values.at("include_path");
  if (parsed.values.contains("define")) result.defines = parsed.values.at("define");
  if (parsed.values.contains("undef")) result.undefs = parsed.values.at("undef");
  if (parsed.values.contains("config")) result.config_overrides = parsed.values.at("config");
  if (parsed.values.contains("repl_syntax")) result.syntax_file = parsed.values.at("repl_syntax").back();
  if (parsed.values.contains("repl_history")) result.history_file = parsed.values.at("repl_history").back();
  if (parsed.values.contains("trace_file")) result.trace_file = parsed.values.at("trace_file").back();
  if (parsed.values.contains("trace_format")) result.trace_format = parsed.values.at("trace_format").back();
  if (parsed.values.contains("dump_symtbl")) result.symtbl_file = parsed.values.at("dump_symtbl").back();
  if (parsed.values.contains("symtbl_format")) result.symtbl_format = parsed.values.at("symtbl_format").back();
  if (!parsed.positionals.empty()) result.input_file = parsed.positionals.front();
  if (parsed.positionals.size() > 1 && !result.output_file) result.output_file = parsed.positionals.at(1);
  return result;
}

}  // namespace ekippx::cli
