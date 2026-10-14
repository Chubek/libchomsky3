/**
 * @file EkippX.hpp
 * @brief Core EkippX macro-expansion runtime, parser facade, diagnostics, and context API.
 *
 * This header defines the value model, source-location model, diagnostics,
 * invocation descriptors, macro registry, expansion context, tracing records,
 * serialization helpers, and convenience entry points used by both embedders and
 * the command-line executable. The API is intentionally host-owned: callers keep
 * control of input buffers, filesystem policy, include roots, plugin loading,
 * recursion limits, and diagnostic collection.
 *
 * @section ekippx_core_lifetime Lifetime
 * Context objects own registered directives, functions, expanders, conditionals,
 * symbols, and macros. Invocations borrow their argument strings for the duration
 * of a callback. Returned strings are value-owned by the caller.
 *
 * @section ekippx_core_errors Error behavior
 * Recoverable failures are reported through Diagnostic values and status codes.
 * Programming or policy violations may throw Error-derived exceptions. Public
 * helpers prefer deterministic diagnostics over process termination.
 */
#pragma once

#include "DSLtk/DSLtk.hpp"
#include "SerdeTk.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ekippx {

using String = std::string;
using StringView = std::string_view;
using Path = std::filesystem::path;
using StringList = std::vector<std::string>;
using StringMap = std::unordered_map<std::string, std::string>;
using ByteBuffer = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;

enum class StatusCode {
  ok,
  warning,
  parse_error,
  lex_error,
  macro_error,
  directive_error,
  function_error,
  expander_error,
  include_error,
  io_error,
  config_error,
  plugin_error,
  limit_error,
  internal_error,
};

enum class DiagnosticLevel { note, warning, error, fatal };
enum class TokenKind {
  text,
  whitespace,
  newline,
  comment,
  directive,
  function,
  expander,
  string_single,
  string_double,
  literal_quoted,
  number,
  identifier,
  atomic,
  delimiter,
  end_of_file,
};

enum class NodeKind {
  document,
  text,
  directive_call,
  function_call,
  expander_call,
  macro_definition,
  macro_invocation,
  conditional_block,
  loop_block,
  include_statement,
  capture_block,
  atomic,
  list_literal,
  string_literal,
};

enum class ValueKind {
  null_value,
  boolean,
  integer,
  floating,
  string,
  literal_string,
  token,
  list,
  map,
  path,
};

enum class SymbolKind { macro_object, macro_function, symbol, constant, alias, counter, builtin };
enum class ScopeKind { global, file, block, macro, loop, capture, temporary, plugin };
enum class ExpansionMode { eager, lazy, literal, disabled };
enum class PatternMode { literal, glob, regex };
enum class NewlineMode { preserve, lf, crlf, platform };
enum class MissingSymbolPolicy { empty, leave_as_is, warn, error };
enum class IncludePolicy { allow, once, sandboxed, deny };
enum class ShellPolicy { inherit, allow, deny };
enum class TruthinessMode { classic, strict, custom };

/**
 * @brief Base exception for all EkippX runtime failures.
 */
class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class LexError : public Error { public: using Error::Error; };
class ParseError : public Error { public: using Error::Error; };
class DirectiveError : public Error { public: using Error::Error; };
class FunctionError : public Error { public: using Error::Error; };
class ExpanderError : public Error { public: using Error::Error; };
class MacroError : public Error { public: using Error::Error; };
class IncludeError : public Error { public: using Error::Error; };
class ConfigError : public Error { public: using Error::Error; };
class SerializationError : public Error { public: using Error::Error; };
class LimitError : public Error { public: using Error::Error; };

/**
 * @brief Represents a source position within an input unit.
 */
struct SourceLocation {
  String source_name{"<memory>"};
  std::size_t line{1};
  std::size_t column{1};
  std::size_t offset{0};
};

/**
 * @brief Represents a half-open source range.
 */
struct SourceRange {
  SourceLocation begin{};
  SourceLocation end{};
};

/**
 * @brief User-facing diagnostic emitted during lexing, parsing, or expansion.
 */
struct Diagnostic {
  DiagnosticLevel level{DiagnosticLevel::note};
  String code{};
  String message{};
  std::optional<SourceRange> range{};
};

/**
 * @brief Lexical token produced by the parser facade.
 */
struct Token {
  TokenKind kind{TokenKind::text};
  String text{};
  SourceRange range{};
};

/**
 * @brief Lightweight dynamic value used for configuration and plugin metadata.
 */
struct Value {
  using Storage = std::variant<std::monostate, bool, std::int64_t, double, String, StringList, StringMap, Path>;
  Storage storage{};

  Value() = default;
  Value(bool v) : storage(v) {}
  Value(std::int64_t v) : storage(v) {}
  Value(int v) : storage(static_cast<std::int64_t>(v)) {}
  Value(double v) : storage(v) {}
  Value(const char* v) : storage(String(v)) {}
  Value(String v) : storage(std::move(v)) {}
  Value(StringList v) : storage(std::move(v)) {}
  Value(StringMap v) : storage(std::move(v)) {}
  Value(Path v) : storage(std::move(v)) {}

  [[nodiscard]] ValueKind kind() const {
    if (std::holds_alternative<std::monostate>(storage)) return ValueKind::null_value;
    if (std::holds_alternative<bool>(storage)) return ValueKind::boolean;
    if (std::holds_alternative<std::int64_t>(storage)) return ValueKind::integer;
    if (std::holds_alternative<double>(storage)) return ValueKind::floating;
    if (std::holds_alternative<String>(storage)) return ValueKind::string;
    if (std::holds_alternative<StringList>(storage)) return ValueKind::list;
    if (std::holds_alternative<StringMap>(storage)) return ValueKind::map;
    return ValueKind::path;
  }

  template <typename T>
  [[nodiscard]] bool is() const {
    return std::holds_alternative<T>(storage);
  }

  template <typename T>
  [[nodiscard]] const T& as() const {
    return std::get<T>(storage);
  }

  [[nodiscard]] String to_string() const {
    if (auto value = std::get_if<std::monostate>(&storage)) {
      (void)value;
      return {};
    }
    if (auto value = std::get_if<bool>(&storage)) return *value ? "true" : "false";
    if (auto value = std::get_if<std::int64_t>(&storage)) return std::to_string(*value);
    if (auto value = std::get_if<double>(&storage)) {
      std::ostringstream stream;
      stream << *value;
      return stream.str();
    }
    if (auto value = std::get_if<String>(&storage)) return *value;
    if (auto value = std::get_if<StringList>(&storage)) {
      std::ostringstream stream;
      for (std::size_t index = 0; index < value->size(); ++index) {
        if (index != 0) stream << ",";
        stream << value->at(index);
      }
      return stream.str();
    }
    if (auto value = std::get_if<StringMap>(&storage)) {
      std::ostringstream stream;
      bool first = true;
      for (const auto& [key, mapped] : *value) {
        if (!first) stream << ";";
        first = false;
        stream << key << "=" << mapped;
      }
      return stream.str();
    }
    return std::get<Path>(storage).string();
  }
};

struct Limits {
  std::size_t max_input_size{4 * 1024 * 1024};
  std::size_t max_output_size{16 * 1024 * 1024};
  std::size_t max_include_depth{64};
  std::size_t max_recursion_depth{32};
  std::size_t max_arguments{64};
  std::size_t max_scope_depth{128};
  std::size_t max_trace_depth{256};
  std::size_t max_macro_name_length{256};
  std::size_t max_include_path_count{128};
};

struct SigilConfig {
  String directive_sigil{"@"};
  String function_sigil{"&"};
  String expander_sigil{"$"};
  String directive_lbrack{"("};
  String directive_rbrack{")"};
  String function_lbrack{"("};
  String function_rbrack{")"};
  String expander_lbrack{"{"};
  String expander_rbrack{"}"};
  String quote_left{"`"};
  String quote_right{"`"};
  String separator{","};
  String all_args_separator{" "};
  String list_separator{","};
};

struct LexOptions {
  bool allow_comments{true};
  bool allow_single_quoted_strings{true};
  bool allow_double_quoted_strings{true};
  bool preserve_whitespace{true};
  NewlineMode normalize_newlines{NewlineMode::preserve};
  SigilConfig sigils{};
};

struct ParseOptions {
  PatternMode pattern_mode{PatternMode::glob};
  TruthinessMode truthiness_mode{TruthinessMode::classic};
  bool allow_atomics{true};
  bool allow_plugin_atomics{true};
  bool dsl_enabled{true};
};

struct RuntimeOptions {
  MissingSymbolPolicy missing_symbol_policy{MissingSymbolPolicy::empty};
  MissingSymbolPolicy missing_env_policy{MissingSymbolPolicy::empty};
  IncludePolicy include_policy{IncludePolicy::allow};
  ShellPolicy shell_policy{ShellPolicy::deny};
  bool strict_mode{false};
  bool allow_redefine{true};
  bool export_scoped_symbols{false};
  bool freeze_macros{false};
  bool trace_enabled{false};
};

struct EnvironmentOptions {
  std::vector<Path> include_paths{};
  Path working_directory{std::filesystem::current_path()};
  String stdin_name{"<stdin>"};
  StringMap env_overrides{};
};

struct Config {
  Limits limits{};
  LexOptions lex{};
  ParseOptions parse{};
  RuntimeOptions runtime{};
  EnvironmentOptions environment{};
  std::unordered_map<std::string, Value> metadata{};
};

struct MacroParameters {
  std::vector<std::string> names{};
  bool variadic{false};
};

/**
 * @brief Runtime macro definition.
 */
struct MacroDefinition {
  String name{};
  std::optional<MacroParameters> parameters{};
  String body{};
  bool literal_body{false};
  std::optional<SourceLocation> defined_at{};
  std::unordered_map<std::string, Value> metadata{};
};

/**
 * @brief Parsed invocation record for directives, functions, expanders, and macros.
 */
struct Invocation {
  String callee{};
  std::vector<String> args{};
  std::optional<SourceRange> range{};
};

/**
 * @brief Top-level expansion request.
 */
struct ExpandRequest {
  String source_name{"<memory>"};
  String input{};
  std::optional<Config> config_override{};
};

/**
 * @brief Expansion result including diagnostics.
 */
struct ExpandResult {
  String output{};
  std::vector<Diagnostic> diagnostics{};
  bool success{true};
};

enum class TraceEventKind { enter, exit, error, limit, marker };

struct TraceEvent {
  std::size_t id{};
  std::optional<std::size_t> parent{};
  TraceEventKind kind{TraceEventKind::enter};
  String callee{};
  String category{};
  std::vector<String> args{};
  std::optional<SourceRange> range{};
  std::size_t depth{};
  bool success{false};
  String message{};
};

struct SymbolRecord {
  String name{};
  String kind{};
  String provider{};
  std::size_t min_arity{};
  std::optional<std::size_t> max_arity{};
  String summary{};
};

struct SymbolTableDump {
  std::vector<SymbolRecord> symbols{};
};

/**
 * @brief Lex/parse result used by validation-oriented APIs.
 */
struct ParseResult {
  std::vector<Token> tokens{};
  std::vector<Diagnostic> diagnostics{};
  bool success{true};
};

/**
 * @brief Minimal AST node used by the parser facade and debugging tools.
 */
struct AstNode {
  NodeKind kind{NodeKind::text};
  String text{};
  std::vector<AstNode> children{};
  std::optional<SourceRange> range{};
};

struct DirectiveSpec {
  String name{};
  std::size_t min_arity{};
  std::optional<std::size_t> max_arity{};
  ExpansionMode expansion_mode{ExpansionMode::eager};
  bool allow_block_body{false};
  bool allow_raw_arguments{false};
};

struct FunctionSpec {
  String name{};
  std::size_t min_arity{};
  std::optional<std::size_t> max_arity{};
  bool pure{true};
  ExpansionMode expansion_mode{ExpansionMode::eager};
};

struct ExpanderSpec {
  String name{};
  std::size_t min_arity{};
  std::optional<std::size_t> max_arity{};
  bool reads_runtime{true};
};

struct ConditionalSpec {
  String name{};
  std::size_t min_arity{};
  std::optional<std::size_t> max_arity{};
};

class Context;

using DirectiveHandler = std::function<void(Context&, const Invocation&)>;
using FunctionHandler = std::function<String(Context&, const Invocation&)>;
using ExpanderHandler = std::function<String(Context&, const Invocation&)>;
using ConditionalHandler = std::function<bool(Context&, const Invocation&)>;
using MacroLambda = std::function<String(Context&, std::span<const std::string>)>;
using AtomicHandler = std::function<void(Context&, StringView)>;

namespace detail {

inline std::string trim(std::string_view input) {
  const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };
  while (!input.empty() && is_space(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
  while (!input.empty() && is_space(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);
  return std::string(input);
}

inline std::string lower_copy(std::string_view input) {
  std::string result(input);
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return result;
}

inline SourceRange make_range(std::string_view source_name, std::size_t begin, std::size_t end, std::string_view text) {
  std::size_t line = 1;
  std::size_t column = 1;
  for (std::size_t index = 0; index < begin && index < text.size(); ++index) {
    if (text[index] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  SourceLocation first{std::string(source_name), line, column, begin};
  for (std::size_t index = begin; index < end && index < text.size(); ++index) {
    if (text[index] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  SourceLocation last{std::string(source_name), line, column, end};
  return {first, last};
}

inline std::string quote_unwrap(const std::string& token) {
  if (token.size() >= 2) {
    const char first = token.front();
    const char last = token.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'') || (first == '`' && last == '`')) {
      return token.substr(1, token.size() - 2);
    }
  }
  return token;
}

inline auto identifier_parser() {
  using namespace dsl;
  auto start = satisfy([](char character) {
    return std::isalpha(static_cast<unsigned char>(character)) || character == '_' || character == '?';
  }, "identifier-start");
  auto rest = *satisfy([](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) || character == '_' || character == '-' || character == '?' || character == '.';
  }, "identifier-rest");
  return parser([start, rest](dsl::ParsecInput& input) -> dsl::ExpectedResult<std::string> {
    auto first = start(input);
    if (!first) return dsl::ExpectedResult<std::string>::failure(first.error.pos, first.error.kind, first.error.expected);
    auto tail = rest(input);
    if (!tail) return dsl::ExpectedResult<std::string>::failure(tail.error.pos, tail.error.kind, tail.error.expected);
    std::string result(1, *first);
    result.append((*tail).begin(), (*tail).end());
    return result;
  });
}

inline std::vector<std::string> split_arguments(std::string_view input) {
  std::vector<std::string> args;
  std::string current;
  int depth_paren = 0;
  int depth_brace = 0;
  int depth_bracket = 0;
  char quote = '\0';
  bool escaped = false;

  for (char character : input) {
    if (escaped) {
      current.push_back(character);
      escaped = false;
      continue;
    }
    if (character == '\\') {
      escaped = true;
      current.push_back(character);
      continue;
    }
    if (quote != '\0') {
      current.push_back(character);
      if (character == quote) quote = '\0';
      continue;
    }
    if (character == '"' || character == '\'' || character == '`') {
      quote = character;
      current.push_back(character);
      continue;
    }
    if (character == '(') ++depth_paren;
    else if (character == ')') --depth_paren;
    else if (character == '{') ++depth_brace;
    else if (character == '}') --depth_brace;
    else if (character == '[') ++depth_bracket;
    else if (character == ']') --depth_bracket;

    if (character == ',' && depth_paren == 0 && depth_brace == 0 && depth_bracket == 0) {
      args.push_back(trim(current));
      current.clear();
      continue;
    }
    current.push_back(character);
  }

  if (!current.empty() || !args.empty()) args.push_back(trim(current));
  if (args.size() == 1 && args.front().empty()) args.clear();
  return args;
}

inline std::optional<std::size_t> find_matching(std::string_view text, std::size_t start, char open, char close) {
  int depth = 0;
  char quote = '\0';
  bool escaped = false;
  for (std::size_t index = start; index < text.size(); ++index) {
    const char character = text[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (character == '\\') {
      escaped = true;
      continue;
    }
    if (quote != '\0') {
      if (character == quote) quote = '\0';
      continue;
    }
    if (character == '"' || character == '\'' || character == '`') {
      quote = character;
      continue;
    }
    if (character == open) ++depth;
    if (character == close) {
      --depth;
      if (depth == 0) return index;
    }
  }
  return std::nullopt;
}

inline bool is_number(std::string_view input) {
  if (input.empty()) return false;
  bool seen_digit = false;
  bool seen_dot = false;
  std::size_t index = (input.front() == '-' || input.front() == '+') ? 1 : 0;
  for (; index < input.size(); ++index) {
    const char character = input[index];
    if (std::isdigit(static_cast<unsigned char>(character))) {
      seen_digit = true;
      continue;
    }
    if (character == '.' && !seen_dot) {
      seen_dot = true;
      continue;
    }
    return false;
  }
  return seen_digit;
}

inline bool truthy(std::string_view input) {
  const auto lowered = lower_copy(trim(input));
  return !(lowered.empty() || lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off" || lowered == "null");
}

inline std::string replace_all(std::string text, std::string_view needle, std::string_view replacement) {
  if (needle.empty()) return text;
  std::size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string::npos) {
    text.replace(offset, needle.size(), replacement);
    offset += replacement.size();
  }
  return text;
}

inline bool is_identifier(std::string_view input) {
  if (input.empty()) return false;
  auto parser_input = dsl::ParsecInput{input, 0};
  const auto parsed = identifier_parser()(parser_input);
  return parsed && (*parsed).size() == input.size();
}

inline std::string safe_display(std::string_view input, std::size_t limit = 96) {
  std::string out;
  out.reserve(std::min(input.size(), limit));
  for (char character : input.substr(0, limit)) {
    if (character == '\n') out += "\\n";
    else if (character == '\r') out += "\\r";
    else if (character == '\t') out += "\\t";
    else out.push_back(character);
  }
  if (input.size() > limit) out += "...";
  return out;
}

inline bool path_has_traversal(const Path& path) {
  for (const auto& part : path) {
    if (part == "..") return true;
  }
  return false;
}

}  // namespace detail

/**
 * @brief Runtime symbol table for host values and aliases.
 */
class SymbolTable {
public:
  [[nodiscard]] bool contains(StringView name) const {
    return contains_symbol(name) || contains_macro(name);
  }

  [[nodiscard]] bool contains_macro(StringView name) const {
    return macro_names_.contains(std::string(name));
  }

  [[nodiscard]] bool contains_symbol(StringView name) const {
    return symbols_.contains(resolve_alias(name));
  }

  void define_symbol(StringView name, StringView value) {
    if (!detail::is_identifier(name)) throw ConfigError("symbol name must be a non-empty identifier: " + std::string(name));
    symbols_[std::string(name)] = std::string(value);
  }

  void define_constant(StringView name, StringView value) {
    if (!detail::is_identifier(name)) throw ConfigError("constant name must be a non-empty identifier: " + std::string(name));
    constants_.insert(std::string(name));
    symbols_[std::string(name)] = std::string(value);
  }

  void register_macro_name(StringView name) {
    if (!detail::is_identifier(name)) throw MacroError("macro name must be a non-empty identifier: " + std::string(name));
    macro_names_.insert(std::string(name));
  }

  void unregister_macro_name(StringView name) {
    macro_names_.erase(std::string(name));
  }

  void undefine(StringView name) {
    const auto key = std::string(name);
    if (constants_.contains(key)) return;
    symbols_.erase(key);
    aliases_.erase(key);
  }

  [[nodiscard]] String get(StringView name) const {
    const auto key = resolve_alias(name);
    const auto found = symbols_.find(key);
    return found == symbols_.end() ? String{} : found->second;
  }

  void alias(StringView alias_name, StringView target_name) {
    if (!detail::is_identifier(alias_name) || !detail::is_identifier(target_name)) throw ConfigError("alias names must be identifiers");
    aliases_[std::string(alias_name)] = std::string(target_name);
  }

  void unalias(StringView alias_name) {
    aliases_.erase(std::string(alias_name));
  }

  [[nodiscard]] std::vector<std::string> names() const {
    std::vector<std::string> result;
    result.reserve(symbols_.size());
    for (const auto& [name, value] : symbols_) {
      (void)value;
      result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
  }

private:
  [[nodiscard]] std::string resolve_alias(StringView name) const {
    auto current = std::string(name);
    std::unordered_set<std::string> visited;
    while (aliases_.contains(current) && !visited.contains(current)) {
      visited.insert(current);
      current = aliases_.at(current);
    }
    return current;
  }

  StringMap symbols_{};
  StringMap aliases_{};
  std::unordered_set<std::string> constants_{};
  std::unordered_set<std::string> macro_names_{};
};

/**
 * @brief Stores object macros, parameterized macros, and lambda macros.
 */
class MacroRegistry {
public:
  void define_object(StringView name, StringView body) {
    validate_macro_name(name);
    definitions_[std::string(name)] = MacroDefinition{std::string(name), std::nullopt, std::string(body), false, std::nullopt, {}};
    lambdas_.erase(std::string(name));
  }

  void define_literal(StringView name, StringView body) {
    validate_macro_name(name);
    definitions_[std::string(name)] = MacroDefinition{std::string(name), std::nullopt, std::string(body), true, std::nullopt, {}};
    lambdas_.erase(std::string(name));
  }

  void define_function(StringView name, MacroParameters params, StringView body) {
    validate_macro_name(name);
    definitions_[std::string(name)] = MacroDefinition{std::string(name), std::move(params), std::string(body), false, std::nullopt, {}};
    lambdas_.erase(std::string(name));
  }

  void define_lambda(StringView name, MacroLambda fn) {
    validate_macro_name(name);
    if (!fn) throw MacroError("lambda macro handler must not be empty");
    definitions_[std::string(name)] = MacroDefinition{std::string(name), MacroParameters{}, {}, false, std::nullopt, {}};
    lambdas_[std::string(name)] = std::move(fn);
  }

  void redefine(StringView name, StringView body) {
    define_object(name, body);
  }

  void undefine(StringView name) {
    definitions_.erase(std::string(name));
    lambdas_.erase(std::string(name));
  }

  [[nodiscard]] bool contains(StringView name) const {
    return definitions_.contains(std::string(name));
  }

  [[nodiscard]] MacroDefinition definition(StringView name) const {
    return definitions_.at(std::string(name));
  }

  [[nodiscard]] const std::unordered_map<std::string, MacroDefinition>& definitions() const {
    return definitions_;
  }

  String expand(StringView name, std::span<const std::string> args, Context& ctx) const;

private:
  static void validate_macro_name(StringView name) {
    if (!detail::is_identifier(name)) throw MacroError("macro name must be a non-empty identifier: " + std::string(name));
  }

  std::unordered_map<std::string, MacroDefinition> definitions_{};
  std::unordered_map<std::string, MacroLambda> lambdas_{};
  friend class Context;
};

template <typename SpecT, typename HandlerT>
class NamedRegistry {
public:
  void put(SpecT spec, HandlerT handler) {
    if (!detail::is_identifier(spec.name)) throw ConfigError("registry name must be a non-empty identifier: " + spec.name);
    if (!handler) throw ConfigError("registry handler must not be empty for: " + spec.name);
    const auto key = spec.name;
    specs_[key] = std::move(spec);
    handlers_[key] = std::move(handler);
  }

  void erase(StringView name) {
    specs_.erase(std::string(name));
    handlers_.erase(std::string(name));
  }

  [[nodiscard]] bool contains(StringView name) const {
    return specs_.contains(std::string(name));
  }

  [[nodiscard]] const SpecT* find(StringView name) const {
    const auto found = specs_.find(std::string(name));
    return found == specs_.end() ? nullptr : &found->second;
  }

  [[nodiscard]] const HandlerT* handler(StringView name) const {
    const auto found = handlers_.find(std::string(name));
    return found == handlers_.end() ? nullptr : &found->second;
  }

  [[nodiscard]] std::vector<std::string> names() const {
    std::vector<std::string> result;
    result.reserve(specs_.size());
    for (const auto& [name, spec] : specs_) {
      (void)spec;
      result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
  }

private:
  std::unordered_map<std::string, SpecT> specs_{};
  std::unordered_map<std::string, HandlerT> handlers_{};
};

using DirectiveRegistry = NamedRegistry<DirectiveSpec, DirectiveHandler>;
using FunctionRegistry = NamedRegistry<FunctionSpec, FunctionHandler>;
using ExpanderRegistry = NamedRegistry<ExpanderSpec, ExpanderHandler>;
using ConditionalRegistry = NamedRegistry<ConditionalSpec, ConditionalHandler>;

/**
 * @brief Thin lexical and parsing façade built on DSLtk primitives.
 */
class ParserFacade {
public:
  explicit ParserFacade(const Config& config) : config_(config) {}

  [[nodiscard]] std::vector<Token> lex(StringView input, StringView source_name = "<memory>") const {
    std::vector<Token> tokens;
    std::size_t index = 0;
    while (index < input.size()) {
      const char character = input[index];
      const std::size_t begin = index;
      TokenKind kind = TokenKind::text;
      if (character == '\n') {
        ++index;
        kind = TokenKind::newline;
      } else if (std::isspace(static_cast<unsigned char>(character))) {
        while (index < input.size() && std::isspace(static_cast<unsigned char>(input[index])) && input[index] != '\n') ++index;
        kind = TokenKind::whitespace;
      } else if (config_.lex.allow_comments && character == '#' ) {
        while (index < input.size() && input[index] != '\n') ++index;
        kind = TokenKind::comment;
      } else if ((character == '@' || character == '&' || character == '$') && begin + 1 < input.size()) {
        ++index;
        auto id_input = dsl::ParsecInput{input.substr(index), 0};
        auto parsed = detail::identifier_parser()(id_input);
        if (parsed) {
          index += (*parsed).size();
          kind = character == '@' ? TokenKind::directive : (character == '&' ? TokenKind::function : TokenKind::expander);
        }
      } else if (character == '"' || character == '\'') {
        const char quote = character;
        ++index;
        while (index < input.size()) {
          if (input[index] == '\\' && index + 1 < input.size()) {
            index += 2;
            continue;
          }
          if (input[index++] == quote) break;
        }
        kind = quote == '"' ? TokenKind::string_double : TokenKind::string_single;
      } else if (detail::is_number(input.substr(index, 1))) {
        while (index < input.size() && (std::isdigit(static_cast<unsigned char>(input[index])) || input[index] == '.')) ++index;
        kind = TokenKind::number;
      } else if (std::isalpha(static_cast<unsigned char>(character)) || character == '_') {
        while (index < input.size() && (std::isalnum(static_cast<unsigned char>(input[index])) || input[index] == '_' || input[index] == '-')) ++index;
        kind = TokenKind::identifier;
      } else if (std::string_view("(){}[],").find(character) != std::string_view::npos) {
        ++index;
        kind = TokenKind::delimiter;
      } else {
        ++index;
        kind = TokenKind::text;
      }
      tokens.push_back(Token{kind, std::string(input.substr(begin, index - begin)), detail::make_range(source_name, begin, index, input)});
    }
    tokens.push_back(Token{TokenKind::end_of_file, {}, detail::make_range(source_name, input.size(), input.size(), input)});
    return tokens;
  }

  [[nodiscard]] AstNode parse(StringView input, StringView source_name = "<memory>") const {
    AstNode root;
    root.kind = NodeKind::document;
    root.text = std::string(source_name);
    std::size_t cursor = 0;
    while (cursor < input.size()) {
      if (const auto maybe = parse_invocation(input, source_name, cursor)) {
        root.children.push_back(*maybe);
        cursor = detail::find_matching(input, cursor + maybe->text.size() + 1, maybe->kind == NodeKind::expander_call ? '{' : '(', maybe->kind == NodeKind::expander_call ? '}' : ')').value_or(cursor) + 1;
      } else {
        const std::size_t next = find_next_sigiled(input, cursor);
        root.children.push_back(AstNode{NodeKind::text, std::string(input.substr(cursor, next - cursor)), {}, detail::make_range(source_name, cursor, next, input)});
        cursor = next;
      }
    }
    return root;
  }

  [[nodiscard]] bool validate(StringView input, StringView source_name = "<memory>") const {
    try {
      (void)parse(input, source_name);
      return true;
    } catch (...) {
      return false;
    }
  }

private:
  [[nodiscard]] std::size_t find_next_sigiled(StringView input, std::size_t from) const {
    const auto pos = input.find_first_of("@&$", from);
    return pos == std::string_view::npos ? input.size() : pos;
  }

  [[nodiscard]] std::optional<AstNode> parse_invocation(StringView input, StringView source_name, std::size_t index) const {
    if (index >= input.size()) return std::nullopt;
    const char sigil = input[index];
    if (sigil != '@' && sigil != '&' && sigil != '$') return std::nullopt;
    const bool expander = sigil == '$';
    const char open = expander ? '{' : '(';
    const char close = expander ? '}' : ')';
    auto name_input = dsl::ParsecInput{input.substr(index + 1), 0};
    const auto parsed_name = detail::identifier_parser()(name_input);
    if (!parsed_name) return std::nullopt;
    const std::size_t name_end = index + 1 + (*parsed_name).size();
    if (name_end >= input.size() || input[name_end] != open) return std::nullopt;
    const auto closing = detail::find_matching(input, name_end, open, close);
    if (!closing) throw ParseError("unmatched invocation delimiter");
    AstNode node;
    node.kind = sigil == '@' ? NodeKind::directive_call : (sigil == '&' ? NodeKind::function_call : NodeKind::expander_call);
    node.text = *parsed_name;
    node.range = detail::make_range(source_name, index, *closing + 1, input);
    const auto raw_args = input.substr(name_end + 1, *closing - name_end - 1);
    for (const auto& arg : detail::split_arguments(raw_args)) {
      node.children.push_back(AstNode{NodeKind::string_literal, arg, {}, std::nullopt});
    }
    return node;
  }

  const Config& config_;
};

/**
 * @brief Main runtime and execution context for one EkippX session.
 */
class Context {
public:
  Context() = default;
  explicit Context(Config config) : config_(std::move(config)) {}

  [[nodiscard]] Config& config() { return config_; }
  [[nodiscard]] const Config& config() const { return config_; }

  void reset() {
    diagnostics_.clear();
    output_.clear();
    include_once_.clear();
    counters_.clear();
    metadata_.clear();
    symbol_table_ = SymbolTable{};
    macro_registry_ = MacroRegistry{};
    directive_registry_ = DirectiveRegistry{};
    function_registry_ = FunctionRegistry{};
    expander_registry_ = ExpanderRegistry{};
    conditional_registry_ = ConditionalRegistry{};
    plugin_installers_.clear();
    active_plugins_.clear();
    trace_.clear();
    trace_stack_.clear();
    next_trace_id_ = 1;
  }

  void soft_reset() {
    diagnostics_.clear();
    output_.clear();
    trace_.clear();
    trace_stack_.clear();
  }

  [[nodiscard]] ExpandResult expand(const ExpandRequest& request) {
    const auto saved = config_;
    if (request.config_override) config_ = *request.config_override;
    diagnostics_.clear();
    output_.clear();
    ExpandResult result;
    try {
      result.output = expand_text(request.input, request.source_name);
      result.success = !has_errors();
    } catch (const Error& error) {
      add_diagnostic(DiagnosticLevel::error, "runtime", error.what(), std::nullopt);
      result.success = false;
    }
    result.diagnostics = diagnostics_;
    config_ = saved;
    return result;
  }

  [[nodiscard]] String expand_text(StringView input, StringView source_name = "<memory>") {
    if (input.size() > config_.limits.max_input_size) throw LimitError("input exceeds configured maximum size");
    return expand_text_impl(input, source_name, 0);
  }

  [[nodiscard]] ParseResult lex(StringView input, StringView source_name = "<memory>") const {
    ParseResult result;
    result.tokens = ParserFacade(config_).lex(input, source_name);
    return result;
  }

  [[nodiscard]] AstNode parse(StringView input, StringView source_name = "<memory>") const {
    return ParserFacade(config_).parse(input, source_name);
  }

  void emit(StringView text) {
    if (output_.size() + text.size() > config_.limits.max_output_size) throw LimitError("output exceeds configured maximum size");
    output_.append(text);
  }

  void emit_line(StringView text) {
    emit(text);
    emit("\n");
  }

  [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
  void clear_diagnostics() { diagnostics_.clear(); }
  [[nodiscard]] bool has_errors() const {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(), [](const Diagnostic& diagnostic) {
      return diagnostic.level == DiagnosticLevel::error || diagnostic.level == DiagnosticLevel::fatal;
    });
  }

  void add_include_path(const Path& path) {
    if (config_.environment.include_paths.size() >= config_.limits.max_include_path_count) throw LimitError("too many include paths");
    if (path.empty() || detail::path_has_traversal(path)) throw ConfigError("include path must not be empty or contain traversal");
    config_.environment.include_paths.push_back(path.lexically_normal());
  }

  void set_working_directory(const Path& path) {
    if (path.empty()) throw ConfigError("working directory must not be empty");
    config_.environment.working_directory = path.lexically_normal();
  }

  void freeze() { config_.runtime.freeze_macros = true; }
  void thaw() { config_.runtime.freeze_macros = false; }

  [[nodiscard]] SymbolTable& symbols() { return symbol_table_; }
  [[nodiscard]] const SymbolTable& symbols() const { return symbol_table_; }
  [[nodiscard]] MacroRegistry& macros() { return macro_registry_; }
  [[nodiscard]] const MacroRegistry& macros() const { return macro_registry_; }
  [[nodiscard]] DirectiveRegistry& directives() { return directive_registry_; }
  [[nodiscard]] FunctionRegistry& functions() { return function_registry_; }
  [[nodiscard]] ExpanderRegistry& expanders() { return expander_registry_; }
  [[nodiscard]] ConditionalRegistry& conditionals() { return conditional_registry_; }
  [[nodiscard]] const DirectiveRegistry& directives() const { return directive_registry_; }
  [[nodiscard]] const FunctionRegistry& functions() const { return function_registry_; }
  [[nodiscard]] const ExpanderRegistry& expanders() const { return expander_registry_; }
  [[nodiscard]] const ConditionalRegistry& conditionals() const { return conditional_registry_; }

  void define_symbol(StringView name, StringView value) { symbol_table_.define_symbol(name, value); }
  void define_constant(StringView name, StringView value) { symbol_table_.define_constant(name, value); }

  void register_directive(DirectiveSpec spec, DirectiveHandler handler) { directive_registry_.put(std::move(spec), std::move(handler)); }
  void unregister_directive(StringView name) { directive_registry_.erase(name); }
  void register_function(FunctionSpec spec, FunctionHandler handler) { function_registry_.put(std::move(spec), std::move(handler)); }
  void unregister_function(StringView name) { function_registry_.erase(name); }
  void register_expander(ExpanderSpec spec, ExpanderHandler handler) { expander_registry_.put(std::move(spec), std::move(handler)); }
  void unregister_expander(StringView name) { expander_registry_.erase(name); }
  void register_conditional(ConditionalSpec spec, ConditionalHandler handler) { conditional_registry_.put(std::move(spec), std::move(handler)); }
  void unregister_conditional(StringView name) { conditional_registry_.erase(name); }

  void register_plugin_loader(StringView name, std::function<void(Context&)> installer) {
    if (!detail::is_identifier(name)) throw ConfigError("plugin loader name must be an identifier: " + std::string(name));
    if (!installer) throw ConfigError("plugin loader must not be empty: " + std::string(name));
    plugin_installers_[std::string(name)] = std::move(installer);
  }

  [[nodiscard]] bool activate_plugin(StringView name) {
    if (!detail::is_identifier(name)) throw DirectiveError("plugin name must be an identifier: " + std::string(name));
    const std::string key(name);
    if (active_plugins_.contains(key)) return true;
    const auto found = plugin_installers_.find(key);
    if (found == plugin_installers_.end()) return false;
    found->second(*this);
    active_plugins_.insert(key);
    return true;
  }

  [[nodiscard]] bool is_plugin_loaded(StringView name) const {
    return active_plugins_.contains(std::string(name));
  }

  [[nodiscard]] std::vector<String> loaded_plugins() const {
    std::vector<String> result(active_plugins_.begin(), active_plugins_.end());
    std::sort(result.begin(), result.end());
    return result;
  }

  void set_metadata(StringView key, Value value) {
    metadata_[std::string(key)] = std::move(value);
    config_.metadata[std::string(key)] = metadata_[std::string(key)];
  }

  [[nodiscard]] const std::unordered_map<std::string, Value>& metadata() const {
    return metadata_;
  }

  void set_counter(StringView name, std::int64_t value) {
    if (!detail::is_identifier(name)) throw ConfigError("counter name must be an identifier: " + std::string(name));
    counters_[std::string(name)] = value;
  }
  void inc_counter(StringView name, std::int64_t amount = 1) {
    if (!detail::is_identifier(name)) throw ConfigError("counter name must be an identifier: " + std::string(name));
    counters_[std::string(name)] += amount;
  }
  void dec_counter(StringView name, std::int64_t amount = 1) {
    if (!detail::is_identifier(name)) throw ConfigError("counter name must be an identifier: " + std::string(name));
    counters_[std::string(name)] -= amount;
  }
  [[nodiscard]] std::int64_t counter(StringView name) const {
    const auto found = counters_.find(std::string(name));
    return found == counters_.end() ? 0 : found->second;
  }

  [[nodiscard]] String output() const { return output_; }

  void add_diagnostic(DiagnosticLevel level, String code, String message, std::optional<SourceRange> range) {
    diagnostics_.push_back(Diagnostic{level, std::move(code), std::move(message), std::move(range)});
  }

  [[nodiscard]] const std::vector<TraceEvent>& trace() const { return trace_; }
  void clear_trace() {
    trace_.clear();
    trace_stack_.clear();
    next_trace_id_ = 1;
  }

  void trace_marker(StringView label) {
    if (!config_.runtime.trace_enabled) return;
    (void)record_trace(TraceEventKind::marker, "marker", std::string(label), {}, std::nullopt, true, {});
  }

  [[nodiscard]] SymbolTableDump dump_symbols() const {
    SymbolTableDump dump;
    const auto append = [&](String name, String kind, String provider, std::size_t min_arity, std::optional<std::size_t> max_arity) {
      dump.symbols.push_back(SymbolRecord{std::move(name), std::move(kind), std::move(provider), min_arity, max_arity, {}});
    };
    for (const auto& name : directive_registry_.names()) {
      const auto* spec = directive_registry_.find(name);
      append(name, "directive", "runtime", spec ? spec->min_arity : 0, spec ? spec->max_arity : std::nullopt);
    }
    for (const auto& name : function_registry_.names()) {
      const auto* spec = function_registry_.find(name);
      append(name, "function", "runtime", spec ? spec->min_arity : 0, spec ? spec->max_arity : std::nullopt);
    }
    for (const auto& name : expander_registry_.names()) {
      const auto* spec = expander_registry_.find(name);
      append(name, "expander", "runtime", spec ? spec->min_arity : 0, spec ? spec->max_arity : std::nullopt);
    }
    for (const auto& name : conditional_registry_.names()) {
      const auto* spec = conditional_registry_.find(name);
      append(name, "conditional", "runtime", spec ? spec->min_arity : 0, spec ? spec->max_arity : std::nullopt);
    }
    for (const auto& [name, definition] : macro_registry_.definitions()) {
      append(name, definition.parameters ? "macro_function" : "macro_object", "user", 0, std::nullopt);
    }
    for (const auto& name : symbol_table_.names()) append(name, "symbol", "user", 0, 0);
    for (const auto& name : loaded_plugins()) append(name, "plugin", "plugin-loader", 0, 0);
    std::sort(dump.symbols.begin(), dump.symbols.end(), [](const auto& left, const auto& right) {
      return std::tie(left.kind, left.name) < std::tie(right.kind, right.name);
    });
    return dump;
  }

  [[nodiscard]] serdetk::Document symbol_document() const {
    auto root = std::make_shared<serdetk::Object>();
    auto symbols = std::make_shared<serdetk::Array>();
    for (const auto& record : dump_symbols().symbols) {
      auto object = std::make_shared<serdetk::Object>();
      object->set("name", record.name);
      object->set("kind", record.kind);
      object->set("provider", record.provider);
      object->set("min_arity", static_cast<unsigned long long>(record.min_arity));
      object->set("max_arity", record.max_arity ? serdetk::Value(static_cast<unsigned long long>(*record.max_arity)) : serdetk::Value(nullptr));
      symbols->push(serdetk::Value(object));
    }
    root->set("symbols", serdetk::Value(symbols));
    root->set("format", "ekippx-symbol-table-v1");
    return serdetk::Document{serdetk::Value(root)};
  }

  [[nodiscard]] serdetk::Document trace_document() const {
    auto root = std::make_shared<serdetk::Object>();
    auto events = std::make_shared<serdetk::Array>();
    for (const auto& event : trace_) {
      auto object = std::make_shared<serdetk::Object>();
      object->set("id", static_cast<unsigned long long>(event.id));
      object->set("parent", event.parent ? serdetk::Value(static_cast<unsigned long long>(*event.parent)) : serdetk::Value(nullptr));
      object->set("kind", trace_kind_name(event.kind));
      object->set("category", event.category);
      object->set("callee", event.callee);
      object->set("depth", static_cast<unsigned long long>(event.depth));
      object->set("success", event.success);
      object->set("message", event.message);
      auto args = std::make_shared<serdetk::Array>();
      for (const auto& arg : event.args) args->push(detail::safe_display(arg));
      object->set("args", serdetk::Value(args));
      events->push(serdetk::Value(object));
    }
    root->set("events", serdetk::Value(events));
    root->set("format", "ekippx-trace-v1");
    return serdetk::Document{serdetk::Value(root)};
  }

  [[nodiscard]] bool evaluate_conditional(StringView expression, const std::optional<SourceRange>& range = std::nullopt) {
    const auto trimmed = detail::trim(expression);
    if (trimmed.empty()) return false;
    const auto open = trimmed.find('(');
    const auto close = trimmed.rfind(')');
    if (open != std::string::npos && close != std::string::npos && close > open) {
      Invocation invocation;
      invocation.callee = detail::trim(trimmed.substr(0, open));
      invocation.args = detail::split_arguments(trimmed.substr(open + 1, close - open - 1));
      invocation.range = range;
      if (const auto* handler = conditional_registry_.handler(invocation.callee)) {
        return (*handler)(*this, invocation);
      }
    }
    std::istringstream stream(trimmed);
    std::string name;
    stream >> name;
    if (!name.empty() && conditional_registry_.contains(name)) {
      std::string remainder;
      std::getline(stream, remainder);
      Invocation invocation;
      invocation.callee = name;
      invocation.args = detail::split_arguments(remainder);
      if (invocation.args.empty() && !detail::trim(remainder).empty()) invocation.args = {detail::trim(remainder)};
      invocation.range = range;
      if (const auto* handler = conditional_registry_.handler(invocation.callee)) {
        return (*handler)(*this, invocation);
      }
    }
    return detail::truthy(trimmed);
  }

private:
  [[nodiscard]] String expand_text_impl(StringView input, StringView source_name, std::size_t depth) {
    if (depth > config_.limits.max_recursion_depth) throw LimitError("maximum recursion depth exceeded");
    std::string result;
    std::size_t cursor = 0;
    std::size_t steps = 0;
    while (cursor < input.size()) {
      if (++steps > config_.limits.max_output_size) {
        (void)record_trace(TraceEventKind::limit, "runtime", "step-limit", {}, std::nullopt, false, "expansion step limit exceeded");
        throw LimitError("expansion step limit exceeded");
      }
      const char sigil = input[cursor];
      if (sigil != '@' && sigil != '&' && sigil != '$') {
        result.push_back(input[cursor++]);
        continue;
      }

      auto name_input = dsl::ParsecInput{input.substr(cursor + 1), 0};
      auto parsed_name = detail::identifier_parser()(name_input);
      if (!parsed_name) {
        result.push_back(input[cursor++]);
        continue;
      }

      const bool expander = sigil == '$';
      const char open = expander ? '{' : '(';
      const char close = expander ? '}' : ')';
      const std::size_t name_size = (*parsed_name).size();
      const std::size_t open_index = cursor + 1 + name_size;
      if (open_index >= input.size() || input[open_index] != open) {
        result.push_back(input[cursor++]);
        continue;
      }
      const auto close_index = detail::find_matching(input, open_index, open, close);
      if (!close_index) throw ParseError("unterminated invocation");

      Invocation invocation;
      invocation.callee = *parsed_name;
      invocation.args = detail::split_arguments(input.substr(open_index + 1, *close_index - open_index - 1));
      invocation.range = detail::make_range(source_name, cursor, *close_index + 1, input);
      if (invocation.args.size() > config_.limits.max_arguments) throw LimitError("too many invocation arguments for: " + invocation.callee);

      for (auto& arg : invocation.args) {
        arg = expand_text_impl(arg, source_name, depth + 1);
        arg = detail::quote_unwrap(arg);
      }

      const auto previous_output_size = output_.size();
      const char* category = sigil == '@' ? "directive" : (sigil == '&' ? "function" : "expander");
      const auto trace_id = record_trace(TraceEventKind::enter, category, invocation.callee, invocation.args, invocation.range, false, {});
      try {
        if (sigil == '@') {
          invoke_directive(invocation);
          result.append(output_.substr(previous_output_size));
        } else if (sigil == '&') {
          result.append(invoke_function(invocation));
        } else {
          result.append(invoke_expander(invocation));
        }
        finish_trace(trace_id, true, {});
      } catch (const Error& error) {
        finish_trace(trace_id, false, error.what());
        throw;
      }
      cursor = *close_index + 1;
    }
    return result;
  }

  void invoke_directive(const Invocation& invocation) {
    const auto* spec = directive_registry_.find(invocation.callee);
    const auto* handler = directive_registry_.handler(invocation.callee);
    if (!spec || !handler) throw DirectiveError("unknown directive: " + invocation.callee);
    validate_arity(invocation, spec->min_arity, spec->max_arity, "directive");
    (*handler)(*this, invocation);
  }

  [[nodiscard]] String invoke_function(const Invocation& invocation) {
    if (macro_registry_.contains(invocation.callee)) {
      return macro_registry_.expand(invocation.callee, invocation.args, *this);
    }
    const auto* spec = function_registry_.find(invocation.callee);
    const auto* handler = function_registry_.handler(invocation.callee);
    if (!spec || !handler) throw FunctionError("unknown function: " + invocation.callee);
    validate_arity(invocation, spec->min_arity, spec->max_arity, "function");
    return (*handler)(*this, invocation);
  }

  [[nodiscard]] String invoke_expander(const Invocation& invocation) {
    const auto* spec = expander_registry_.find(invocation.callee);
    const auto* handler = expander_registry_.handler(invocation.callee);
    if (!spec || !handler) {
      switch (config_.runtime.missing_symbol_policy) {
        case MissingSymbolPolicy::leave_as_is:
          return "$" + invocation.callee + "{}";
        case MissingSymbolPolicy::warn:
          add_diagnostic(DiagnosticLevel::warning, "missing-expander", "unknown expander: " + invocation.callee, invocation.range);
          return {};
        case MissingSymbolPolicy::error:
          throw ExpanderError("unknown expander: " + invocation.callee);
        case MissingSymbolPolicy::empty:
        default:
          return {};
      }
    }
    validate_arity(invocation, spec->min_arity, spec->max_arity, "expander");
    return (*handler)(*this, invocation);
  }

  static void validate_arity(const Invocation& invocation, std::size_t min_arity, const std::optional<std::size_t>& max_arity, StringView kind) {
    if (invocation.args.size() < min_arity) throw Error(std::string(kind) + " `" + invocation.callee + "` expects at least " + std::to_string(min_arity) + " argument(s)");
    if (max_arity && invocation.args.size() > *max_arity) throw Error(std::string(kind) + " `" + invocation.callee + "` expects at most " + std::to_string(*max_arity) + " argument(s)");
  }

  [[nodiscard]] std::size_t record_trace(TraceEventKind kind, String category, String callee, std::vector<String> args, std::optional<SourceRange> range, bool success, String message) {
    if (!config_.runtime.trace_enabled) return 0;
    if (trace_.size() >= config_.limits.max_trace_depth) throw LimitError("trace event limit exceeded");
    const std::optional<std::size_t> parent = trace_stack_.empty() ? std::nullopt : std::optional<std::size_t>(trace_stack_.back());
    const auto id = next_trace_id_++;
    trace_.push_back(TraceEvent{id, parent, kind, std::move(callee), std::move(category), std::move(args), std::move(range), trace_stack_.size(), success, std::move(message)});
    if (kind == TraceEventKind::enter) trace_stack_.push_back(id);
    return id;
  }

  void finish_trace(std::size_t id, bool success, String message) {
    if (!config_.runtime.trace_enabled || id == 0) return;
    if (!trace_stack_.empty() && trace_stack_.back() == id) trace_stack_.pop_back();
    const auto found = std::find_if(trace_.begin(), trace_.end(), [id](const TraceEvent& event) { return event.id == id; });
    String category = found == trace_.end() ? "runtime" : found->category;
    String callee = found == trace_.end() ? "" : found->callee;
    std::vector<String> args = found == trace_.end() ? std::vector<String>{} : found->args;
    (void)record_trace(success ? TraceEventKind::exit : TraceEventKind::error, std::move(category), std::move(callee), std::move(args), std::nullopt, success, std::move(message));
  }

  static String trace_kind_name(TraceEventKind kind) {
    switch (kind) {
      case TraceEventKind::enter: return "enter";
      case TraceEventKind::exit: return "exit";
      case TraceEventKind::error: return "error";
      case TraceEventKind::limit: return "limit";
      case TraceEventKind::marker: return "marker";
    }
    return "unknown";
  }

  Config config_{};
  std::vector<Diagnostic> diagnostics_{};
  std::string output_{};
  SymbolTable symbol_table_{};
  MacroRegistry macro_registry_{};
  DirectiveRegistry directive_registry_{};
  FunctionRegistry function_registry_{};
  ExpanderRegistry expander_registry_{};
  ConditionalRegistry conditional_registry_{};
  std::unordered_map<std::string, std::function<void(Context&)>> plugin_installers_{};
  std::unordered_set<std::string> active_plugins_{};
  std::unordered_set<std::string> include_once_{};
  std::unordered_map<std::string, std::int64_t> counters_{};
  std::unordered_map<std::string, Value> metadata_{};
  std::vector<TraceEvent> trace_{};
  std::vector<std::size_t> trace_stack_{};
  std::size_t next_trace_id_{1};
};

inline String MacroRegistry::expand(StringView name, std::span<const std::string> args, Context& ctx) const {
  const auto found_definition = definitions_.find(std::string(name));
  if (found_definition == definitions_.end()) throw MacroError("unknown macro: " + std::string(name));
  const auto found_lambda = lambdas_.find(std::string(name));
  if (found_lambda != lambdas_.end()) return found_lambda->second(ctx, args);

  std::string body = found_definition->second.body;
  for (std::size_t index = 0; index < args.size(); ++index) {
    body = detail::replace_all(std::move(body), "$" + std::to_string(index + 1), args[index]);
  }
  body = detail::replace_all(std::move(body), "$#", std::to_string(args.size()));
  body = detail::replace_all(std::move(body), "$@", [&]() {
    std::ostringstream stream;
    for (std::size_t index = 0; index < args.size(); ++index) {
      if (index != 0) stream << ctx.config().lex.sigils.all_args_separator;
      stream << args[index];
    }
    return stream.str();
  }());
  return found_definition->second.literal_body ? body : ctx.expand_text(body);
}

/**
 * @brief Returns a baseline context with no batteries attached.
 */
inline Context default_context() {
  return Context{};
}

namespace dsl {

template <std::size_t N>
struct FixedString {
  char value[N]{};

  constexpr FixedString(const char (&text)[N]) {
    for (std::size_t index = 0; index < N; ++index) value[index] = text[index];
  }

  [[nodiscard]] constexpr std::string_view view() const { return std::string_view(value, N - 1); }
  [[nodiscard]] constexpr bool operator==(const FixedString&) const = default;
};

template <typename Derived, typename... Features>
struct DSL : Features... {
protected:
  [[nodiscard]] constexpr Derived& self() noexcept { return static_cast<Derived&>(*this); }
  [[nodiscard]] constexpr const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }
};

struct Operators {
  template <typename Derived>
  struct Mixin {};

  template <typename F>
  static constexpr auto make_pred(F&& function) {
    return std::forward<F>(function);
  }
};

template <FixedString PatternText>
struct pattern {
  static constexpr auto value = PatternText;
  [[nodiscard]] static bool matches(std::string_view input) { return input == value.view(); }
};

struct RuleContext {
  ekippx::Context* runtime{nullptr};
};

struct Predicate {
  std::function<bool(std::string_view)> test{};
  [[nodiscard]] bool operator()(std::string_view input) const { return test ? test(input) : false; }
};

struct Rule {
  std::string name{};
  Predicate guard{};
  std::function<std::string(RuleContext&, std::string_view)> transform{};
  [[nodiscard]] bool matches(std::string_view input) const { return !guard.test || guard(input); }
  [[nodiscard]] std::string apply(RuleContext& context, std::string_view input) const {
    if (!transform) throw ConfigError("DSL rule has no transform: " + name);
    if (!matches(input)) throw ConfigError("DSL rule guard rejected input: " + name);
    return transform(context, input);
  }
};

inline Predicate when_contains(std::string needle) {
  return Predicate{[needle = std::move(needle)](std::string_view input) { return input.find(needle) != std::string_view::npos; }};
}

inline Rule rewrite(std::string name, Predicate guard, std::function<std::string(RuleContext&, std::string_view)> transform) {
  if (!detail::is_identifier(name)) throw ConfigError("DSL rule name must be an identifier: " + name);
  return Rule{std::move(name), std::move(guard), std::move(transform)};
}

}  // namespace dsl

namespace literals {}
namespace config {}
namespace ast {}
namespace runtime {}
namespace registry {}

}  // namespace ekippx
