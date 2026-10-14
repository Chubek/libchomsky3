/**
 * @file EkippX-CLI.cpp
 * @brief Command-line driver for batch expansion, evaluation, tracing, symbol dumps, and REPL mode.
 *
 * The executable constructs a batteries-enabled Context, applies Klyspec-backed
 * command-line options, optionally loads static example plugins, expands input
 * from files or inline expressions, and delegates interactive sessions to the
 * PikoRL bridge. All user-visible failures are translated into diagnostics and
 * non-zero process status codes.
 */
#include "EkippX-Batteries.hpp"
#include "KlyspecBridge.hpp"

#include <fstream>
#include <iostream>

namespace ekippx::cli {

int run_pikorl_bridge();

enum class ExitCode {
  success = 0,
  usage_error = 2,
  config_error = 3,
  input_error = 4,
  parse_error = 5,
  runtime_error = 6,
  io_error = 7,
  internal_error = 99,
};

struct CLIConfig {
  bool interactive{false};
  bool show_help{false};
  bool show_version{false};
  bool check_only{false};
  bool dump_tokens{false};
  bool dump_ast{false};
  bool dump_config{false};
  bool trace{false};
  bool quiet{false};
  bool verbose{false};
  std::optional<std::string> eval_text{};
  std::optional<Path> input_file{};
  std::optional<Path> output_file{};
  std::optional<Path> syntax_file{};
  std::optional<Path> history_file{};
  std::optional<Path> trace_file{};
  std::optional<Path> symtbl_file{};
  std::string trace_format{"json"};
  std::string symtbl_format{"json"};
  std::vector<std::string> include_paths{};
  std::vector<std::string> defines{};
  std::vector<std::string> undefs{};
  std::vector<std::string> config_overrides{};
};

inline bool should_launch_repl(int argc, char** argv) {
  if (argc <= 1) return true;
  for (int index = 1; index < argc; ++index) {
    const std::string_view token = argv[index];
    if (token == "-i" || token == "--interactive") return true;
    if (token == "-h" || token == "--help" || token == "-V" || token == "--version") return false;
  }
  return false;
}

class ArgumentParser {
public:
  [[nodiscard]] CLIConfig parse(int argc, char** argv) const {
    const auto parsed = parse_cli_with_klyspec(argc, argv);
    if (!parsed.ok) throw CLIError(parsed.message);

    CLIConfig config;
    config.show_help = parsed.show_help;
    config.show_version = parsed.show_version;
    config.interactive = should_launch_repl(argc, argv) || parsed.interactive;
    config.check_only = parsed.check_only;
    config.dump_tokens = parsed.dump_tokens;
    config.dump_ast = parsed.dump_ast;
    config.dump_config = parsed.dump_config;
    config.trace = parsed.trace;
    config.quiet = parsed.quiet;
    config.verbose = parsed.verbose;
    config.eval_text = parsed.eval_text;
    config.input_file = parsed.input_file;
    config.output_file = parsed.output_file;
    config.syntax_file = parsed.syntax_file;
    config.history_file = parsed.history_file;
    config.trace_file = parsed.trace_file;
    config.symtbl_file = parsed.symtbl_file;
    config.trace_format = parsed.trace_format;
    config.symtbl_format = parsed.symtbl_format;
    config.include_paths = parsed.include_paths;
    config.defines = parsed.defines;
    config.undefs = parsed.undefs;
    config.config_overrides = parsed.config_overrides;
    return config;
  }

  [[nodiscard]] std::string usage() const {
    return
        "ekippx-cli [OPTIONS] [INPUT] [OUTPUT]\n"
        "  -i, --interactive      launch the REPL\n"
        "  -e, --eval TEXT        expand literal text\n"
        "  -o, --output FILE      write output to FILE\n"
        "  -I, --include-path P   add include search path\n"
        "  -D, --define NAME=VAL  predefine a symbol or macro\n"
        "  -U, --undef NAME       remove a symbol or macro\n"
        "      --check            validate without writing output\n"
        "      --dump-tokens      dump the token stream\n"
        "      --dump-ast         dump a readable AST\n"
        "      --dump-symtbl FILE dump registered symbols\n"
        "      --symtbl-format F  symbol dump format: json|msgpack\n"
        "      --trace            record expansion trace\n"
        "      --trace-file FILE  write expansion trace\n"
        "      --trace-format F   trace format: json|msgpack\n"
        "      --repl-syntax FILE override the REPL syntax file\n"
        "  -V, --version          print version\n"
        "  -h, --help             print help";
  }

  [[nodiscard]] std::string version_text() const {
    return "EkippX 0.1-pass1";
  }

  [[nodiscard]] bool wants_repl(int argc, char** argv) const {
    return should_launch_repl(argc, argv);
  }

private:
  class CLIError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
  };
};

inline Path default_syntax_file() {
  return Path("EkippX.syntax");
}

inline Path default_history_file() {
  return Path(".ekippx_history");
}

inline void attach_batteries(ekippx::Context& ctx) {
  batteries::register_all(ctx);
}

inline std::string dump_ast(const AstNode& node, int depth = 0) {
  std::ostringstream stream;
  stream << std::string(depth * 2, ' ') << node.text << " [" << static_cast<int>(node.kind) << "]";
  for (const auto& child : node.children) stream << "\n" << dump_ast(child, depth + 1);
  return stream.str();
}

inline std::string lower_ext(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

inline std::string infer_format(std::string requested, const std::optional<Path>& path) {
  requested = lower_ext(requested);
  if (requested != "auto") return requested;
  if (!path) return "json";
  const auto ext = lower_ext(path->extension().string());
  if (ext == ".msgpack" || ext == ".mpack" || ext == ".bin") return "msgpack";
  return "json";
}

inline bool safe_output_path(const Path& path) {
  return !path.empty() && !ekippx::detail::path_has_traversal(path);
}

inline void write_document(const serdetk::Document& document, const std::optional<Path>& path, std::string format_name) {
  format_name = infer_format(std::move(format_name), path);
  serdetk::CompiledFormat format = format_name == "msgpack" ? serdetk::builtins::messagepack() : serdetk::builtins::json();
  if (!path) {
    if (format.is_binary()) throw SerializationError("binary serialization requires an output file");
    std::cout << format.dump_string(document);
    return;
  }
  if (!safe_output_path(*path)) throw SerializationError("output path must not be empty or contain traversal: " + path->string());
  format.dump_file(document, *path);
}

class CLIApplication {
public:
  CLIApplication() : context_(batteries::batteries_context()) {}

  [[nodiscard]] ekippx::Context& context() { return context_; }
  [[nodiscard]] const ekippx::Context& context() const { return context_; }

  [[nodiscard]] int run_main(int argc, char** argv) {
    try {
      auto result = run(argc, argv);
      return static_cast<int>(result);
    } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << "\n";
      return static_cast<int>(ExitCode::internal_error);
    }
  }

  [[nodiscard]] ExitCode run(int argc, char** argv) {
    const ArgumentParser parser;
    const auto config = parser.parse(argc, argv);
    if (config.show_help) {
      std::cout << parser.usage() << "\n";
      return ExitCode::success;
    }
    if (config.show_version) {
      std::cout << parser.version_text() << "\n";
      return ExitCode::success;
    }
    return run(config);
  }

  [[nodiscard]] ExitCode run(const CLIConfig& config) {
    apply_config(config);
    if (config.interactive) return run_repl(config);
    return run_batch(config);
  }

  [[nodiscard]] ExitCode run_batch(const CLIConfig& config) {
    std::string input;
    std::string source_name = "<memory>";
    if (config.eval_text) {
      input = *config.eval_text;
      source_name = "<eval>";
    } else if (config.input_file) {
      source_name = config.input_file->string();
      std::ifstream file(*config.input_file);
      if (!file) {
        std::cerr << "error: failed to read " << source_name << "\n";
        return ExitCode::io_error;
      }
      input.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    } else {
      input.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
      source_name = context_.config().environment.stdin_name;
    }

    if (config.dump_tokens) {
      for (const auto& token : context_.lex(input, source_name).tokens) {
        std::cout << static_cast<int>(token.kind) << " " << token.text << "\n";
      }
      return ExitCode::success;
    }
    if (config.dump_ast) {
      std::cout << dump_ast(context_.parse(input, source_name)) << "\n";
      return ExitCode::success;
    }
    if (config.dump_config) {
      std::cout << "strict=" << (context_.config().runtime.strict_mode ? "true" : "false") << "\n";
      std::cout << "trace=" << (context_.config().runtime.trace_enabled ? "true" : "false") << "\n";
      std::cout << "include_paths=" << context_.config().environment.include_paths.size() << "\n";
      return ExitCode::success;
    }
    if (config.symtbl_file) {
      write_document(context_.symbol_document(), config.symtbl_file, config.symtbl_format);
      return ExitCode::success;
    }

    const auto expanded = context_.expand(ExpandRequest{source_name, input, std::nullopt});
    for (const auto& diagnostic : expanded.diagnostics) {
      if (!config.quiet) std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    if (!expanded.success || config.check_only) return expanded.success ? ExitCode::success : ExitCode::runtime_error;

    if (config.trace || config.trace_file) write_document(context_.trace_document(), config.trace_file, config.trace_format);

    if (config.output_file) {
      if (!safe_output_path(*config.output_file)) return ExitCode::io_error;
      std::ofstream file(*config.output_file);
      if (!file) return ExitCode::io_error;
      file << expanded.output;
    } else {
      std::cout << expanded.output;
      if (!expanded.output.empty() && expanded.output.back() != '\n') std::cout << "\n";
    }
    return ExitCode::success;
  }

  [[nodiscard]] ExitCode run_repl(const CLIConfig& config) {
    if (!config.quiet) {
      std::cout << "EkippX REPL\n";
      std::cout << "PikoRL backend with ekippx_expand, ekippx_trace, and ekippx_symbols bindings.\n";
      std::cout << "Syntax file: " << (config.syntax_file ? config.syntax_file->string() : default_syntax_file().string()) << "\n";
    }
    return run_pikorl_bridge() == 0 ? ExitCode::success : ExitCode::runtime_error;
  }

private:
  void apply_config(const CLIConfig& config) {
    context_.config().runtime.trace_enabled = config.trace || config.trace_file.has_value();
    for (const auto& include_path : config.include_paths) context_.add_include_path(include_path);
    for (const auto& item : config.undefs) {
      context_.macros().undefine(item);
      context_.symbols().undefine(item);
    }
    for (const auto& item : config.defines) {
      const auto split = item.find('=');
      if (split == std::string::npos) {
        context_.define_symbol(item, "1");
      } else {
        context_.define_symbol(item.substr(0, split), item.substr(split + 1));
      }
    }
    for (const auto& item : config.config_overrides) {
      const auto split = item.find('=');
      if (split == std::string::npos) continue;
      const Invocation inv{"config", {item.substr(0, split), item.substr(split + 1)}, std::nullopt};
      (*context_.directives().handler("config"))(context_, inv);
    }
  }

  ekippx::Context context_;
};

}  // namespace ekippx::cli

int main(int argc, char** argv) {
  return ekippx::cli::CLIApplication{}.run_main(argc, argv);
}
