#pragma once

#include "IPCtk/DSLUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace klyspec {

enum class ArgumentKind {
  flag,
  switch_,
  option,
  positional,
  variadic,
  repeatable,
  key_value,
};

enum class ValuePolicy {
  none,
  optional,
  required,
};

struct ArgumentSpec {
  std::string id{};
  ArgumentKind kind{ArgumentKind::flag};
  ValuePolicy value_policy{ValuePolicy::none};
  std::vector<std::string> names{};
  std::string help{};
  std::optional<std::string> default_value{};
  bool required{false};
};

struct OptionSpec : ArgumentSpec {
  OptionSpec() {
    kind = ArgumentKind::option;
    value_policy = ValuePolicy::required;
  }
};

struct PositionalSpec : ArgumentSpec {
  std::size_t index{0};
  PositionalSpec() {
    kind = ArgumentKind::positional;
    value_policy = ValuePolicy::required;
  }
};

struct CommandSpec {
  std::string name{};
  std::string help{};
  std::vector<std::string> aliases{};
  std::vector<ArgumentSpec> arguments{};
};

struct ParseResult {
  std::unordered_map<std::string, std::vector<std::string>> values{};
  std::vector<std::string> positionals{};
  std::vector<std::string> diagnostics{};
  bool ok{true};
};

struct SourceSpan {
  std::size_t begin{0};
  std::size_t end{0};
  std::size_t line{1};
  std::size_t column{1};
};

struct Diagnostic {
  SourceSpan span{};
  std::string message{};
};

struct KlytmkNode {
  std::string kind{};
  std::string name{};
  std::string body{};
  std::unordered_map<std::string, std::string> attributes{};
  SourceSpan span{};
};

struct KlytmkAst {
  std::vector<KlytmkNode> nodes{};
};

struct KlytmkParseResult {
  std::optional<KlytmkAst> ast{};
  std::vector<Diagnostic> diagnostics{};
  bool ok{false};
};

class Registry {
public:
  bool register_command(CommandSpec spec) {
    if (spec.name.empty() || commands_.contains(spec.name)) {
      return false;
    }
    const auto name = spec.name;
    commands_.emplace(name, std::move(spec));
    aliases_[name] = name;
    return true;
  }

  bool register_alias(std::string_view command_name, std::string alias) {
    if (!commands_.contains(std::string(command_name)) || alias.empty() || aliases_.contains(alias)) {
      return false;
    }
    aliases_[std::move(alias)] = std::string(command_name);
    return true;
  }

  bool register_argument(std::string_view command_name, ArgumentSpec spec) {
    auto *cmd = find_command_mutable(command_name);
    if (cmd == nullptr || spec.id.empty()) {
      return false;
    }

    for (const auto &arg : cmd->arguments) {
      if (arg.id == spec.id) {
        return false;
      }
      for (const auto &name : spec.names) {
        if (std::find(arg.names.begin(), arg.names.end(), name) != arg.names.end()) {
          return false;
        }
      }
    }

    cmd->arguments.push_back(std::move(spec));
    return true;
  }

  const CommandSpec *lookup(std::string_view name_or_alias) const {
    const auto alias_it = aliases_.find(std::string(name_or_alias));
    if (alias_it == aliases_.end()) {
      return nullptr;
    }
    const auto cmd_it = commands_.find(alias_it->second);
    return cmd_it == commands_.end() ? nullptr : &cmd_it->second;
  }


  const ArgumentSpec *lookup_argument(std::string_view command_name, std::string_view id_or_name) const {
    const auto *command = lookup(command_name);
    if (command == nullptr) {
      return nullptr;
    }

    for (const auto &arg : command->arguments) {
      if (arg.id == id_or_name) {
        return &arg;
      }
      for (const auto &name : arg.names) {
        if (name == id_or_name) {
          return &arg;
        }
      }
    }

    return nullptr;
  }

private:
  CommandSpec *find_command_mutable(std::string_view name_or_alias) {
    auto alias_it = aliases_.find(std::string(name_or_alias));
    if (alias_it == aliases_.end()) {
      return nullptr;
    }

    auto cmd_it = commands_.find(alias_it->second);
    return cmd_it == commands_.end() ? nullptr : &cmd_it->second;
  }

  std::unordered_map<std::string, CommandSpec> commands_{};
  std::unordered_map<std::string, std::string> aliases_{};
  friend class KlyCLIService;
};

class KlyCLIService {
public:
  explicit KlyCLIService(const Registry &registry) : registry_(registry) {}

  ParseResult parse(std::string_view command_name, const std::vector<std::string> &argv) const {
    ParseResult result{};
    const auto *command = registry_.lookup(command_name);
    if (command == nullptr) {
      result.ok = false;
      result.diagnostics.push_back("unknown command");
      return result;
    }

    bool end_of_options = false;
    for (std::size_t i = 0; i < argv.size(); ++i) {
      const auto &token = argv[i];
      if (!end_of_options && token == "--") {
        end_of_options = true;
        continue;
      }

      if (!end_of_options && token.size() > 2 && token.rfind("--", 0) == 0) {
        auto long_name = token;
        std::optional<std::string> inline_value{};
        const auto eq = token.find('=');
        if (eq != std::string::npos) {
          long_name = token.substr(0, eq);
          inline_value = token.substr(eq + 1);
        }

        const auto *arg = registry_.lookup_argument(command->name, long_name);
        if (arg == nullptr) {
          result.ok = false;
          result.diagnostics.push_back("unknown option: " + long_name);
          continue;
        }
        if (arg->value_policy == ValuePolicy::required) {
          if (inline_value.has_value()) {
            result.values[arg->id].push_back(*inline_value);
            continue;
          }
          if (i + 1 >= argv.size()) {
            result.ok = false;
            result.diagnostics.push_back("missing value for option: " + long_name);
            continue;
          }
          result.values[arg->id].push_back(argv[++i]);
        } else {
          if (inline_value.has_value()) {
            result.values[arg->id].push_back(*inline_value);
          } else {
            result.values[arg->id].push_back("true");
          }
        }
        continue;
      }

      if (!end_of_options && token.size() > 1 && token[0] == '-' && token[1] != '-') {
        for (std::size_t j = 1; j < token.size(); ++j) {
          const std::string short_name = std::string("-") + token[j];
          const auto *arg = registry_.lookup_argument(command->name, short_name);
          if (arg == nullptr) {
            result.ok = false;
            result.diagnostics.push_back("unknown option: " + short_name);
            continue;
          }
          if (arg->value_policy == ValuePolicy::required) {
            if (j + 1 < token.size()) {
              result.values[arg->id].push_back(token.substr(j + 1));
              break;
            }
            if (i + 1 >= argv.size()) {
              result.ok = false;
              result.diagnostics.push_back("missing value for option: " + short_name);
              break;
            }
            result.values[arg->id].push_back(argv[++i]);
            break;
          }
          result.values[arg->id].push_back("true");
        }
        continue;
      }

      result.positionals.push_back(token);
    }

    for (const auto &arg : command->arguments) {
      if (arg.required && !result.values.contains(arg.id)) {
        result.ok = false;
        result.diagnostics.push_back("missing required argument: " + arg.id);
      }
      if (!result.values.contains(arg.id) && arg.default_value.has_value()) {
        result.values[arg.id].push_back(*arg.default_value);
      }
      if (arg.kind == ArgumentKind::flag && result.values.contains(arg.id) && result.values.at(arg.id).size() > 1) {
        result.ok = false;
        result.diagnostics.push_back("flag provided multiple times: " + arg.id);
      }
      if ((arg.kind == ArgumentKind::option || arg.kind == ArgumentKind::switch_) &&
          arg.kind != ArgumentKind::repeatable && result.values.contains(arg.id) &&
          result.values.at(arg.id).size() > 1) {
        result.ok = false;
        result.diagnostics.push_back("non-repeatable option provided multiple times: " + arg.id);
      }
    }

    // Validate positional/variadic contracts declared in ArgumentSpec list.
    std::vector<const ArgumentSpec *> positional_specs{};
    positional_specs.reserve(command->arguments.size());
    const ArgumentSpec *variadic = nullptr;
    for (const auto &arg : command->arguments) {
      if (arg.kind == ArgumentKind::positional) {
        positional_specs.push_back(&arg);
      } else if (arg.kind == ArgumentKind::variadic) {
        variadic = &arg;
      }
    }
    if (result.positionals.size() < positional_specs.size()) {
      result.ok = false;
      result.diagnostics.push_back("missing positional argument(s)");
    }
    if (variadic == nullptr && result.positionals.size() > positional_specs.size() && !positional_specs.empty()) {
      result.ok = false;
      result.diagnostics.push_back("unexpected extra positional argument(s)");
    }

    return result;
  }

private:
  const Registry &registry_;
};

namespace detail {
inline auto line_col(std::string_view source, std::size_t pos) -> std::pair<std::size_t, std::size_t> {
  std::size_t line = 1;
  std::size_t col = 1;
  for (std::size_t i = 0; i < pos && i < source.size(); ++i) {
    if (source[i] == '\n') {
      ++line;
      col = 1;
    } else {
      ++col;
    }
  }
  return {line, col};
}

inline auto consume_block(dsl::ParsecInput &in, std::string &content) -> bool {
  if (in.eof() || in.peek() != '{') {
    return false;
  }
  in.consume();
  int depth = 1;
  while (!in.eof() && depth > 0) {
    const char c = in.consume();
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
    }
    if (depth > 0) {
      content.push_back(c);
    }
  }
  return depth == 0;
}

inline auto parse_help_string(std::string_view block) -> std::optional<std::string> {
  const auto key = block.find("help-string");
  if (key == std::string_view::npos) {
    return std::nullopt;
  }
  const auto open = block.find('{', key);
  const auto close = block.find('}', open == std::string_view::npos ? key : open + 1);
  if (open == std::string_view::npos || close == std::string_view::npos || close <= open) {
    return std::nullopt;
  }
  std::string text(block.substr(open + 1, close - open - 1));
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}
} // namespace detail


inline auto parse_klytmk(std::string_view source) -> KlytmkParseResult {
  std::string owned_source(source);
  std::string_view src_view(owned_source);
  auto ws = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<char> {
    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) {
      in.consume();
    }
    return '\0';
  });
  auto token = [&](char c) {
    return dsl::parser([&, c](dsl::ParsecInput &in) -> dsl::ExpectedResult<char> {
      ws(in);
      if (!in.eof() && in.peek() == c) {
        return in.consume();
      }
      return dsl::fail_expected<char>(in, std::string(1, c));
    });
  };
  auto ident = dsl::parser([&](dsl::ParsecInput &in) -> dsl::ExpectedResult<std::string> {
    ws(in);
    if (in.eof() || !(std::isalpha(static_cast<unsigned char>(in.peek())) || in.peek() == '-')) {
      return dsl::fail_expected<std::string>(in, "identifier");
    }
    std::string out;
    out.push_back(in.consume());
    while (!in.eof() && (std::isalnum(static_cast<unsigned char>(in.peek())) || in.peek() == '-' || in.peek() == '_')) {
      out.push_back(in.consume());
    }
    return out;
  });
  auto quoted = dsl::parser([&](dsl::ParsecInput &in) -> dsl::ExpectedResult<std::string> {
    ws(in);
    if (in.eof() || in.peek() != '"') {
      return dsl::fail_expected<std::string>(in, "quoted-string");
    }
    in.consume();
    std::string out;
    while (!in.eof() && in.peek() != '"') {
      out.push_back(in.consume());
    }
    if (in.eof()) {
      return dsl::fail_expected<std::string>(in, "closing-quote");
    }
    in.consume();
    return out;
  });

  dsl::ParsecInput in{owned_source, 0};
  KlytmkAst ast{};
  bool parse_failed = false;
  std::string parse_error = "klytmk parse error";
  std::size_t parse_error_pos = 0;
  auto fail = [&](std::string msg, std::size_t pos) {
    parse_failed = true;
    parse_error = std::move(msg);
    parse_error_pos = pos;
  };
  while (!in.eof()) {
    ws(in);
    if (in.eof()) {
      break;
    }
    const auto begin = in.pos;
    auto kind = ident(in);
    if (!kind) {
      fail("expected top-level identifier", in.pos);
      break;
    }
    if (*kind == "param" || *kind == "command") {
      auto name = quoted(in);
      if (!name) {
        fail("expected quoted node name", in.pos);
        break;
      }
      ws(in);
      std::string body;
      if (!detail::consume_block(in, body)) {
        fail("expected block body", in.pos);
        break;
      }
      if (!token(';')(in)) {
        fail("expected ';' after block", in.pos);
        break;
      }
      const auto [line, col] = detail::line_col(src_view, begin);
      std::unordered_map<std::string, std::string> attrs{};
      if (auto help_text = detail::parse_help_string(body)) {
        attrs["help-string"] = *help_text;
      }
      ast.nodes.push_back(KlytmkNode{
          .kind = *kind, .name = *name, .body = std::move(body), .attributes = std::move(attrs), .span = SourceSpan{begin, in.pos, line, col}});
    } else if (*kind == "pre-evaluate") {
      if (!token('{')(in)) {
        fail("expected '{' after pre-evaluate", in.pos);
        break;
      }
      std::unordered_map<std::string, std::string> attrs{};
      while (!in.eof()) {
        ws(in);
        if (!in.eof() && in.peek() == '}') {
          in.consume();
          break;
        }
        auto key = ident(in);
        if (!key || !token('=')(in)) {
          fail("expected key=value pair in pre-evaluate", in.pos);
          break;
        }
        auto value = quoted(in);
        if (!value || !token(';')(in)) {
          fail("expected quoted value and ';' in pre-evaluate", in.pos);
          break;
        }
        attrs[*key] = *value;
      }
      if (!token(';')(in)) {
        fail("expected ';' after pre-evaluate block", in.pos);
        break;
      }
      const auto [line, col] = detail::line_col(src_view, begin);
      ast.nodes.push_back(KlytmkNode{.kind = *kind, .attributes = std::move(attrs), .span = SourceSpan{begin, in.pos, line, col}});
    } else {
      fail("unknown top-level directive: " + *kind, begin);
      break;
    }
  }

  KlytmkParseResult result{};
  if (in.eof() && !parse_failed) {
    result.ok = true;
    result.ast = std::move(ast);
  } else {
    const auto pos = parse_failed ? parse_error_pos : in.pos;
    const auto [line, col] = detail::line_col(src_view, pos);
    result.diagnostics.push_back(Diagnostic{.span = SourceSpan{pos, pos + 1, line, col}, .message = parse_error});
    result.ok = false;
  }
  return result;
}

} // namespace klyspec
