/**
 * @file Parzek.cpp
 * @brief Parzek grammar tokenizer, parser, preprocessor, code generator, and CLI implementation.
 *
 * This translation unit contains the internal grammar AST, preprocessor state,
 * parser combinators, C++ emitter, and command-line entry point used by the
 * public functions declared in Parzek.hpp. Internal helpers report errors through
 * CompileResult diagnostics instead of terminating the process.
 */
#include "Parzek.hpp"

#include "DSLtk.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <ranges>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace parzek {
namespace {

using dsl::ExpectedResult;
using dsl::ParsecInput;
using dsl::Parser;

struct RuleRefExpr {
  std::string name;
};
struct LiteralExpr {
  std::string value;
};
struct ClassExpr {
  std::string chars;
};

struct Expr;
using ExprVariant = std::variant<RuleRefExpr, LiteralExpr, ClassExpr>;

struct Expr {
  ExprVariant node;
  std::vector<Expr> sequence;
  std::vector<Expr> alternatives;
  bool one_or_more{false};
  bool zero_or_more{false};
  bool optional{false};
  bool adjacent{false};
  std::optional<std::string> when_guard;
};

struct Rule {
  std::string name;
  Expr expr;
  bool lexical{false};
  std::optional<std::string> channel;
};

struct Grammar {
  std::string parser_name{"Generated"};
  std::optional<std::string> parser_start;
  std::vector<Rule> rules;
};

struct Token {
  enum class Kind {
    Identifier,
    String,
    Class,
    Symbol,
    Channel,
    End
  } kind{Kind::End};
  std::string text;
  std::size_t line{1};
  std::size_t column{1};
};

struct PreprocessorState {
  std::unordered_map<std::string, std::pair<std::vector<std::string>, std::string>> macros;
  std::unordered_map<std::string, std::string> meta_vars{{"JOINER", " "}, {"SEPARATOR", " "}, {"CONVFMT", "%g"}, {"SHELL", "/bin/sh"}, {"CURR", ""}};
  int last_status{0};
  std::string last_eval{"0"};
};

static std::pair<std::size_t, std::size_t> line_col(std::string_view text, std::size_t pos) {
  std::size_t line = 1, col = 1;
  for (std::size_t i = 0; i < pos && i < text.size(); ++i) {
    if (text[i] == '\n') {
      ++line;
      col = 1;
    } else {
      ++col;
    }
  }
  return {line, col};
}

static void push_diag(CompileResult& result, std::string msg, std::string file, std::size_t line, std::size_t column, DiagnosticSeverity sev = DiagnosticSeverity::Error) {
  result.diagnostics.push_back(Diagnostic{sev, std::move(msg), std::move(file), line, column});
}

static std::string strip_comments(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size();) {
    if (i + 1 < input.size() && input[i] == '/' && input[i + 1] == '/') {
      i += 2;
      while (i < input.size() && input[i] != '\n') {
        ++i;
      }
      continue;
    }
    if (i + 1 < input.size() && input[i] == '/' && input[i + 1] == '*') {
      i += 2;
      while (i + 1 < input.size() && !(input[i] == '*' && input[i + 1] == '/')) {
        ++i;
      }
      if (i + 1 < input.size()) i += 2;
      continue;
    }
    out.push_back(input[i++]);
  }
  return out;
}

static std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

static std::vector<std::string> split_args(std::string_view inside) {
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;
  for (char c : inside) {
    if (c == '"') in_quotes = !in_quotes;
    if (c == ',' && !in_quotes) {
      args.push_back(trim(current));
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty()) args.push_back(trim(current));
  return args;
}

static std::string simple_eval_expr(const std::string& expr) {
  std::istringstream in(expr);
  long long acc = 0;
  long long v = 0;
  char op = '+';
  while (in >> v) {
    if (op == '+') acc += v;
    else if (op == '-') acc -= v;
    in >> op;
  }
  return std::to_string(acc);
}

static std::string replace_all(std::string input, std::string_view from, std::string_view to) {
  std::size_t pos = 0;
  while ((pos = input.find(from, pos)) != std::string::npos) {
    input.replace(pos, from.size(), to);
    pos += to.size();
  }
  return input;
}

static std::string expand_macro_body(const std::string& body, const std::vector<std::string>& args, PreprocessorState& pp, std::unordered_set<std::string>& hide_set, const std::string& macro_name);

static std::string expand_text(const std::string& text, PreprocessorState& pp, std::unordered_set<std::string> hide_set = {}) {
  std::string out;
  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '+' && i + 1 < text.size() &&
        (std::isalpha(static_cast<unsigned char>(text[i + 1])) || text[i + 1] == '_')) {
      ++i;
      std::size_t start = i;
      while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '.' || text[i] == ':')) ++i;
      out += text.substr(start, i - start);
      if (i < text.size() && text[i] == '(') {
        int depth = 0;
        std::size_t call_start = i;
        for (; i < text.size(); ++i) {
          if (text[i] == '(') ++depth;
          if (text[i] == ')') {
            --depth;
            if (depth == 0) {
              ++i;
              break;
            }
          }
        }
        out += text.substr(call_start, i - call_start);
      }
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_' ) {
      std::size_t start = i;
      while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '.' || text[i] == ':')) ++i;
      std::string name = text.substr(start, i - start);
      if (i < text.size() && text[i] == '(' && pp.macros.contains(name) && !hide_set.contains(name)) {
        int depth = 0;
        std::size_t arg_start = i + 1;
        std::size_t j = i;
        for (; j < text.size(); ++j) {
          if (text[j] == '(') ++depth;
          if (text[j] == ')') {
            --depth;
            if (depth == 0) break;
          }
        }
        auto args = split_args(text.substr(arg_start, j - arg_start));
        hide_set.insert(name);
        auto expanded = expand_macro_body(pp.macros[name].second, args, pp, hide_set, name);
        hide_set.erase(name);
        out += expanded;
        i = j + 1;
        continue;
      }
      if (!name.empty() && name[0] == '+') {
        out += name.substr(1);
      } else {
        out += name;
      }
      continue;
    }
    out.push_back(text[i++]);
  }
  return out;
}

static std::string expand_macro_body(const std::string& body, const std::vector<std::string>& args, PreprocessorState& pp, std::unordered_set<std::string>& hide_set, const std::string&) {
  std::string out = body;
  std::vector<std::pair<std::string, std::string>> noexpand_tokens;
  auto protect = [&](const std::string& text) -> std::string {
    std::string key = "__PARZEK_NOEXP_" + std::to_string(noexpand_tokens.size()) + "__";
    noexpand_tokens.push_back({key, text});
    return key;
  };
  for (std::size_t i = 0; i < args.size(); ++i) {
    const auto idx = std::to_string(i + 1);
    out = replace_all(out, "!#" + idx, expand_text(args[i], pp, hide_set));
    out = replace_all(out, "#" + idx + "!", protect(args[i]));
    out = replace_all(out, "#" + idx, expand_text(args[i], pp, hide_set));
  }
  std::string args_joined;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i) args_joined += pp.meta_vars["JOINER"];
    args_joined += expand_text(args[i], pp, hide_set);
  }
  std::string raw_joined;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i) raw_joined += pp.meta_vars["JOINER"];
    raw_joined += args[i];
  }
  out = replace_all(out, "#@", args_joined);
  out = replace_all(out, "#!", protect(raw_joined));
  if (!args.empty()) {
    std::string first = trim(args[0]);
    std::istringstream in(first);
    std::string tok;
    if (in >> tok) out = replace_all(out, "##", tok);
  }
  out = replace_all(out, "#?", std::to_string(pp.last_status));
  out = replace_all(out, "#*", pp.last_eval);
  out = replace_all(out, "#0", "");
  out = replace_all(out, "++", "");
  out = expand_text(out, pp, hide_set);
  for (const auto& [key, raw] : noexpand_tokens) {
    out = replace_all(out, key, raw);
  }
  return out;
}

static std::string preprocess(std::string_view src, CompileResult& result, const std::string& source_name) {
  PreprocessorState pp;
  std::stringstream in(strip_comments(src));
  std::string line;
  std::string output;
  while (std::getline(in, line)) {
    auto t = trim(line);
    if (t.empty()) {
      output += "\n";
      continue;
    }
    if (t.rfind("@define", 0) == 0) {
      auto after = trim(t.substr(7));
      auto lparen = after.find('(');
      auto rparen = after.find(')');
      if (lparen == std::string::npos || rparen == std::string::npos || rparen < lparen) {
        push_diag(result, "Malformed @define", source_name, 1, 1);
        continue;
      }
      auto name = trim(after.substr(0, lparen));
      if (!name.empty() && (name[0] == '.' || name[0] == ':')) {
        push_diag(result, "Macro names cannot start with '.' or ':'", source_name, 1, 1);
        continue;
      }
      auto params_raw = after.substr(lparen + 1, rparen - lparen - 1);
      auto body = trim(after.substr(rparen + 1));
      pp.macros[name] = {split_args(params_raw), body};
      continue;
    }
    if (t.rfind("@parser:name", 0) == 0) {
      output += t;
      output += "\n";
      continue;
    }
    if (t.rfind("@eval", 0) == 0) {
      auto l = t.find('(');
      auto r = t.rfind(')');
      if (l != std::string::npos && r != std::string::npos && r > l) {
        pp.last_eval = simple_eval_expr(t.substr(l + 1, r - l - 1));
      }
      continue;
    }
    if (t.rfind("@system", 0) == 0 || t.rfind("@exec", 0) == 0 || t.rfind("@printf", 0) == 0 || t.rfind("@sprintf", 0) == 0 || t.rfind("@foreach", 0) == 0) {
      pp.last_status = 0;
      continue;
    }
    output += expand_text(t, pp);
    output += "\n";
  }
  return output;
}

class Tokenizer {
 public:
  explicit Tokenizer(std::string_view src) : source_(src) {}

  std::vector<Token> tokenize() {
    std::vector<Token> tokens;
    while (pos_ < source_.size()) {
      auto c = source_[pos_];
      if (std::isspace(static_cast<unsigned char>(c))) {
        consume_ws();
        continue;
      }
      if (c == '"') {
        tokens.push_back(read_string());
        continue;
      }
      if (c == '[') {
        tokens.push_back(read_class());
        continue;
      }
      if (c == '@') {
        tokens.push_back(read_channel());
        continue;
      }
      if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        tokens.push_back(read_identifier());
        continue;
      }
      tokens.push_back(read_symbol());
    }
    tokens.push_back(Token{Token::Kind::End, "", line_, col_});
    return tokens;
  }

 private:
  void advance() {
    if (source_[pos_] == '\n') {
      ++line_;
      col_ = 1;
    } else {
      ++col_;
    }
    ++pos_;
  }

  void consume_ws() {
    while (pos_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[pos_]))) {
      advance();
    }
  }

  Token read_identifier() {
    auto l = line_, c = col_;
    std::string text;
    while (pos_ < source_.size()) {
      char ch = source_[pos_];
      if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) break;
      text.push_back(ch);
      advance();
    }
    return Token{Token::Kind::Identifier, text, l, c};
  }

  Token read_string() {
    auto l = line_, c = col_;
    std::string text;
    advance();
    while (pos_ < source_.size() && source_[pos_] != '"') {
      if (source_[pos_] == '\\' && pos_ + 1 < source_.size()) {
        text.push_back(source_[pos_ + 1]);
        advance();
        advance();
        continue;
      }
      text.push_back(source_[pos_]);
      advance();
    }
    if (pos_ < source_.size()) advance();
    return Token{Token::Kind::String, text, l, c};
  }

  Token read_class() {
    auto l = line_, c = col_;
    std::string text;
    advance();
    while (pos_ < source_.size() && source_[pos_] != ']') {
      text.push_back(source_[pos_]);
      advance();
    }
    if (pos_ < source_.size()) advance();
    return Token{Token::Kind::Class, text, l, c};
  }

  Token read_channel() {
    auto l = line_, c = col_;
    std::string text;
    text.push_back(source_[pos_]);
    advance();
    while (pos_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_')) {
      text.push_back(source_[pos_]);
      advance();
    }
    return Token{Token::Kind::Channel, text, l, c};
  }

  Token read_symbol() {
    auto l = line_, c = col_;
    std::string text(1, source_[pos_]);
    advance();
    if ((text == "-" || text == "=") && pos_ < source_.size() && source_[pos_] == '>') {
      text.push_back('>');
      advance();
    } else if ((text == "=" || text == "!") && pos_ < source_.size() && source_[pos_] == '=') {
      text.push_back('=');
      advance();
    }
    return Token{Token::Kind::Symbol, text, l, c};
  }

  std::string source_;
  std::size_t pos_{0};
  std::size_t line_{1};
  std::size_t col_{1};
};

class GrammarParser {
 public:
  GrammarParser(std::vector<Token> tokens, CompileResult& result, std::string source_name)
      : tokens_(std::move(tokens)), result_(result), source_name_(std::move(source_name)) {}

  std::optional<Grammar> parse() {
    Grammar grammar;
    while (!match(Token::Kind::End)) {
      if (peek().kind == Token::Kind::Channel && peek().text == "@parser") {
        auto save = index_;
        advance();
        if (match_symbol(":") && peek().kind == Token::Kind::Identifier) {
          auto directive = advance().text;
          if (!expect("(")) return std::nullopt;
          auto value = expect_identifier();
          if (value.empty()) return std::nullopt;
          if (!expect(")")) return std::nullopt;
          if (directive == "name") {
            grammar.parser_name = value;
            continue;
          }
          if (directive == "start") {
            grammar.parser_start = value;
            continue;
          }
          diag_here("Unsupported @parser directive: " + directive);
          return std::nullopt;
        }
        index_ = save;
      }
      auto rule = parse_rule();
      if (!rule) return std::nullopt;
      grammar.rules.push_back(std::move(*rule));
    }
    if (grammar.rules.empty()) {
      diag_here("No grammar rules found");
      return std::nullopt;
    }
    return grammar;
  }

 private:
  std::optional<Rule> parse_rule() {
    auto name = expect_identifier();
    if (name.empty()) return std::nullopt;
    if (!expect(":")) return std::nullopt;
    auto expr = parse_expression();
    if (!expr) return std::nullopt;

    std::optional<std::string> channel;
    if (match_symbol("->")) {
      if (peek().kind == Token::Kind::Channel) {
        channel = advance().text;
      } else {
        diag_here("Expected channel name like @IGNORE after ->");
        return std::nullopt;
      }
    }

    if (!expect(";")) return std::nullopt;
    Rule rule;
    rule.name = name;
    rule.expr = std::move(*expr);
    rule.lexical = is_lexical(name);
    rule.channel = std::move(channel);
    if (!validate_rule_name(rule)) {
      return std::nullopt;
    }
    return rule;
  }

  bool validate_rule_name(const Rule& rule) {
    if (rule.lexical) {
      bool ok = std::ranges::all_of(rule.name, [](char c) { return std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'; });
      if (!ok) {
        diag_here("Lexical rule names must be ALL_CAPS");
        return false;
      }
      return true;
    }
    bool ok = std::ranges::all_of(rule.name, [](char c) { return std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '-'; });
    if (!ok) {
      diag_here("Syntactic rule names must be kebab-case");
      return false;
    }
    return true;
  }

  std::optional<Expr> parse_expression() {
    auto first = parse_sequence();
    if (!first) return std::nullopt;
    std::vector<Expr> alts;
    alts.push_back(*first);
    while (match_symbol("|")) {
      auto next = parse_sequence();
      if (!next) return std::nullopt;
      alts.push_back(*next);
    }
    if (alts.size() == 1) return alts.front();
    Expr e;
    e.alternatives = std::move(alts);
    return e;
  }

  std::optional<Expr> parse_sequence() {
    std::vector<Expr> seq;
    bool previous_adjacent = false;
    while (true) {
      if (peek().kind == Token::Kind::Symbol && (peek().text == ")" || peek().text == ";" || peek().text == "|" || peek().text == "->")) break;
      auto atom = parse_postfix();
      if (!atom) break;
      atom->adjacent = previous_adjacent;
      seq.push_back(*atom);
      previous_adjacent = match_symbol("~");
      if (peek().kind == Token::Kind::Identifier && peek().text == "when") {
        advance();
        std::ostringstream guard;
        while (!(peek().kind == Token::Kind::Symbol && (peek().text == ";" || peek().text == ")" || peek().text == "|"))) {
          if (peek().kind == Token::Kind::End) break;
          auto tok = advance();
          guard << tok.text;
          guard << ' ';
        }
        seq.back().when_guard = trim(guard.str());
      }
    }
    if (seq.empty()) {
      diag_here("Expected expression");
      return std::nullopt;
    }
    if (seq.size() == 1) return seq.front();
    Expr e;
    e.sequence = std::move(seq);
    return e;
  }

  std::optional<Expr> parse_postfix() {
    auto base = parse_primary();
    if (!base) return std::nullopt;
    while (peek().kind == Token::Kind::Symbol && (peek().text == "*" || peek().text == "+" || peek().text == "?")) {
      auto op = advance().text;
      if (op == "*") base->zero_or_more = true;
      if (op == "+") base->one_or_more = true;
      if (op == "?") base->optional = true;
    }
    return base;
  }

  std::optional<Expr> parse_primary() {
    if (match_symbol("(")) {
      auto expr = parse_expression();
      if (!expr) return std::nullopt;
      if (!expect(")")) return std::nullopt;
      return expr;
    }
    if (peek().kind == Token::Kind::String) {
      Expr e;
      e.node = LiteralExpr{advance().text};
      return e;
    }
    if (peek().kind == Token::Kind::Class) {
      Expr e;
      e.node = ClassExpr{advance().text};
      return e;
    }
    if (peek().kind == Token::Kind::Identifier) {
      Expr e;
      e.node = RuleRefExpr{advance().text};
      return e;
    }
    return std::nullopt;
  }

  bool is_lexical(const std::string& name) {
    return !name.empty() && std::isupper(static_cast<unsigned char>(name.front()));
  }

  Token& peek() { return tokens_[index_]; }

  Token advance() {
    if (index_ < tokens_.size()) ++index_;
    return tokens_[index_ - 1];
  }

  bool match(Token::Kind kind) { return peek().kind == kind; }

  bool match_symbol(const std::string& symbol) {
    if (peek().kind == Token::Kind::Symbol && peek().text == symbol) {
      advance();
      return true;
    }
    return false;
  }

  std::string expect_identifier() {
    if (peek().kind != Token::Kind::Identifier) {
      diag_here("Expected identifier");
      return {};
    }
    return advance().text;
  }

  bool expect(const std::string& symbol) {
    if (!(peek().kind == Token::Kind::Symbol && peek().text == symbol)) {
      diag_here("Expected '" + symbol + "'");
      return false;
    }
    advance();
    return true;
  }

  void diag_here(const std::string& msg) {
    const auto& t = peek();
    push_diag(result_, msg, source_name_, t.line, t.column);
  }

  std::vector<Token> tokens_;
  std::size_t index_{0};
  CompileResult& result_;
  std::string source_name_;
};

static std::string sanitize_symbol(std::string s) {
  for (char& c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) c = '_';
  }
  if (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) s = "R_" + s;
  return s;
}

static std::string map_rule_name_to_fn(const std::string& name) {
  std::string out;
  for (char c : name) {
    if (c == '-') out.push_back('_');
    else out.push_back(c);
  }
  return sanitize_symbol(out);
}

static std::string cxx_string_literal(const std::string& s) {
  std::string out{"\""};
  for (char c : s) {
    if (c == '\\' || c == '"') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

static std::string expr_to_cpp(const Expr& expr, const std::unordered_set<std::string>& lexical_rules, bool allow_skip);

static std::string wrap_quantifiers(const Expr& expr, std::string base) {
  if (expr.one_or_more) {
    base = "parzek_support::many1(" + base + ")";
  }
  if (expr.zero_or_more) {
    base = "*" + base;
  }
  if (expr.optional) {
    base = "dsl::optional(" + base + ")";
  }
  if (expr.when_guard) {
    base = "parzek_support::when_guard(" + base + ", ctx_, " + cxx_string_literal(*expr.when_guard) + ")";
  }
  return base;
}

static std::string leaf_to_cpp(const Expr& expr, const std::unordered_set<std::string>& lexical_rules, bool allow_skip) {
  std::string base;
  if (std::holds_alternative<LiteralExpr>(expr.node)) {
    auto lit = std::get<LiteralExpr>(expr.node).value;
    base = "parzek_support::token(" + cxx_string_literal(lit) + ", " + (allow_skip ? "true" : "false") + ", &ctx_)";
  } else if (std::holds_alternative<ClassExpr>(expr.node)) {
    auto cls = std::get<ClassExpr>(expr.node).chars;
    base = "parzek_support::char_class(" + cxx_string_literal(cls) + ")";
  } else if (std::holds_alternative<RuleRefExpr>(expr.node)) {
    auto ref = std::get<RuleRefExpr>(expr.node).name;
    if (lexical_rules.contains(ref)) {
      base = "parse_" + map_rule_name_to_fn(ref) + "()";
    } else {
      base = "parse_" + map_rule_name_to_fn(ref) + "()";
    }
  } else {
    base = "dsl::parser([](dsl::ParsecInput&)->dsl::ExpectedResult<std::string>{ return std::nullopt; })";
  }
  return wrap_quantifiers(expr, base);
}

static std::string expr_to_cpp(const Expr& expr, const std::unordered_set<std::string>& lexical_rules, bool allow_skip) {
  if (!expr.alternatives.empty()) {
    std::string code = "parzek_support::lift(" + expr_to_cpp(expr.alternatives[0], lexical_rules, allow_skip) + ")";
    for (std::size_t i = 1; i < expr.alternatives.size(); ++i) {
      code += " | parzek_support::lift(" + expr_to_cpp(expr.alternatives[i], lexical_rules, allow_skip) + ")";
    }
    return wrap_quantifiers(expr, code);
  }
  if (!expr.sequence.empty()) {
    std::string code = "parzek_support::lift(" + expr_to_cpp(expr.sequence[0], lexical_rules, allow_skip && !expr.sequence[0].adjacent) + ")";
    for (std::size_t i = 1; i < expr.sequence.size(); ++i) {
      code = "parzek_support::seq(" + code + ", parzek_support::lift(" + expr_to_cpp(expr.sequence[i], lexical_rules, allow_skip && !expr.sequence[i].adjacent) + "))";
    }
    return wrap_quantifiers(expr, code);
  }
  return leaf_to_cpp(expr, lexical_rules, allow_skip);
}

static std::string make_header(const Grammar& grammar, const std::string& stem) {
  std::ostringstream out;
  const auto guard = sanitize_symbol(stem + "_PARZEK_HPP");
  out << "#pragma once\n\n";
  out << "#include \"DSLUtils.hpp\"\n";
  out << "#include <optional>\n";
  out << "#include <string>\n";
  out << "#include <string_view>\n";
  out << "#include <vector>\n";
  out << "#include <memory>\n\n";
  out << "namespace " << sanitize_symbol(grammar.parser_name) << "_parzek {\n";
  out << "\n";
  out << "struct ASTNode {\n";
  out << "  std::string rule_name;\n";
  out << "  std::string text;\n";
  out << "  std::vector<std::shared_ptr<ASTNode>> children;\n";
  out << "  size_t line{1};\n";
  out << "  size_t column{1};\n";
  out << "};\n\n";
  out << "struct PredicateContext {\n";
  out << "  std::string file;\n";
  out << "  std::size_t line{1};\n";
  out << "  std::size_t column{1};\n";
  out << "  std::string chan{\"@IGNORE\"};\n";
  out << "  std::string current_text;\n";
  out << "};\n\n";
  out << "namespace parzek_support {\n";
  out << "template <typename T>\n";
  out << "dsl::Parser<T> when_guard(const dsl::Parser<T>& p, PredicateContext& ctx, std::string_view expr);\n";
  out << "dsl::Parser<std::string> token(std::string token_text, bool allow_skip, PredicateContext* ctx = nullptr);\n";
  out << "dsl::Parser<std::string> char_class(std::string chars);\n";
  out << "template <typename T> dsl::Parser<std::string> lift(const dsl::Parser<T>& p);\n";
  out << "dsl::Parser<std::string> seq(const dsl::Parser<std::string>& a, const dsl::Parser<std::string>& b);\n";
  out << "template <typename T> dsl::Parser<std::vector<T>> many1(const dsl::Parser<T>& p);\n";
  out << "template <typename T, typename Open, typename Close> dsl::Parser<T> between(const dsl::Parser<Open>& open, const dsl::Parser<T>& core, const dsl::Parser<Close>& close);\n";
  out << "template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep);\n";
  out << "template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by1(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep);\n";
  out << "}\n\n";
  out << "struct Parser {\n";
  out << "  explicit Parser(std::string source_name = \"<input>\");\n";
  out << "  dsl::ParseOutcome<std::string> parse(std::string_view input);\n";
  out << "  std::shared_ptr<ASTNode> parse_ast(std::string_view input);\n";
  out << "\n";
  for (const auto& rule : grammar.rules) {
    out << "  dsl::Parser<std::string> parse_" << map_rule_name_to_fn(rule.name) << "();\n";
  }
  out << "\n";
  for (const auto& rule : grammar.rules) {
    if (!rule.lexical) {
      out << "  std::shared_ptr<ASTNode> parse_" << map_rule_name_to_fn(rule.name) << "_ast();\n";
    }
  }
  out << "\n";
  out << " private:\n";
  out << "  PredicateContext ctx_;\n";
  out << "  dsl::ParsecInput* active_input_{nullptr};\n";
  out << "  dsl::Parser<std::string> skip_ignored();\n";
  out << "};\n\n";
  out << "}\n";
  return out.str();
}

static std::string make_source(const Grammar& grammar, const std::string& header_name) {
  std::unordered_set<std::string> lexical_rules;
  for (const auto& r : grammar.rules) {
    if (r.lexical) lexical_rules.insert(r.name);
  }

  std::ostringstream out;
  out << "#include \"" << header_name << "\"\n";
  out << "#include <algorithm>\n";
  out << "#include <cctype>\n";
  out << "#include <functional>\n";
  out << "#include <ranges>\n";
  out << "#include <sstream>\n";
  out << "#include <type_traits>\n";
  out << "\n";
  out << "namespace " << sanitize_symbol(grammar.parser_name) << "_parzek {\n\n";

  out << "namespace parzek_support {\n";
  out << "static bool eval_guard(PredicateContext& ctx, std::string_view expr) {\n";
  out << "  std::string e(expr);\n";
  out << "  auto has = [&](std::string_view p){ return e.find(p) != std::string::npos; };\n";
  out << "  if (has(\"LEN\")) {\n";
  out << "    auto pos = e.find(\"!=\");\n";
  out << "    if (pos != std::string::npos) {\n";
  out << "      auto rhs = std::stoll(e.substr(pos + 2));\n";
  out << "      return static_cast<long long>(ctx.current_text.size()) != rhs;\n";
  out << "    }\n";
  out << "    pos = e.find(\"==\");\n";
  out << "    if (pos != std::string::npos) {\n";
  out << "      auto rhs = std::stoll(e.substr(pos + 2));\n";
  out << "      return static_cast<long long>(ctx.current_text.size()) == rhs;\n";
  out << "    }\n";
  out << "  }\n";
  out << "  if (has(\"CHAN\") && has(\"==\")) {\n";
  out << "    auto pos = e.find(\"==\");\n";
  out << "    auto rhs = e.substr(pos + 2);\n";
  out << "    rhs.erase(std::remove_if(rhs.begin(), rhs.end(), [](unsigned char c){ return std::isspace(c); }), rhs.end());\n";
  out << "    return ctx.chan == rhs;\n";
  out << "  }\n";
  out << "  if (has(\"$0\") && has(\"!=\")) {\n";
  out << "    auto quote = e.find('\\'');\n";
  out << "    if (quote != std::string::npos && quote + 2 < e.size()) {\n";
  out << "      auto ch = e[quote + 1];\n";
  out << "      return ctx.current_text.empty() || ctx.current_text[0] != ch;\n";
  out << "    }\n";
  out << "  }\n";
  out << "  return true;\n";
  out << "}\n";

  out << "template <typename T>\n";
  out << "dsl::Parser<T> when_guard(const dsl::Parser<T>& p, PredicateContext& ctx, std::string_view expr) {\n";
  out << "  return dsl::parser([p, &ctx, expr](dsl::ParsecInput& in)->dsl::ExpectedResult<T>{\n";
  out << "    auto save = in.pos;\n";
  out << "    auto r = p(in);\n";
  out << "    if (!r) return r;\n";
  out << "    if (!eval_guard(ctx, expr)) {\n";
  out << "      in.pos = save;\n";
  out << "      return dsl::ExpectedResult<T>::failure(save, dsl::ParseFailureKind::Soft, {\"when\"});\n";
  out << "    }\n";
  out << "    return r;\n";
  out << "  });\n";
  out << "}\n";

  out << "dsl::Parser<std::string> token(std::string token_text, bool allow_skip, PredicateContext* ctx) {\n";
  out << "  return dsl::parser([token_text = std::move(token_text), allow_skip, ctx](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{\n";
  out << "    auto start = in.pos;\n";
  out << "    std::size_t skipped = 0;\n";
  out << "    if (allow_skip) {\n";
  out << "      while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) { in.consume(); ++skipped; }\n";
  out << "      if (ctx) ctx->chan = skipped > 0 ? \"@IGNORE\" : \"@TOK\";\n";
  out << "    }\n";
  out << "    for (char c : token_text) {\n";
  out << "      if (in.eof() || in.peek() != c) {\n";
  out << "        in.pos = start;\n";
  out << "        if (ctx) ctx->chan = \"@TOK\";\n";
  out << "        return dsl::ExpectedResult<std::string>::failure(start, dsl::ParseFailureKind::Soft, {token_text});\n";
  out << "      }\n";
  out << "      in.consume();\n";
  out << "    }\n";
  out << "    if (ctx) { ctx->current_text = token_text; if (!allow_skip) ctx->chan = \"@TOK\"; }\n";
  out << "    return token_text;\n";
  out << "  });\n";
  out << "}\n";

  out << "dsl::Parser<std::string> char_class(std::string chars) {\n";
  out << "  return dsl::parser([chars = std::move(chars)](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{\n";
  out << "    if (in.eof()) return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Soft, {\"char-class\"});\n";
  out << "    char c = in.peek();\n";
  out << "    bool ok = false;\n";
  out << "    for (std::size_t i = 0; i < chars.size(); ++i) {\n";
  out << "      if (i + 2 < chars.size() && chars[i + 1] == '-') {\n";
  out << "        if (c >= chars[i] && c <= chars[i + 2]) ok = true;\n";
  out << "        i += 2;\n";
  out << "      } else if (chars[i] == c) ok = true;\n";
  out << "    }\n";
  out << "    if (!ok) return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Soft, {\"char-class\"});\n";
  out << "    in.consume();\n";
  out << "    return std::string(1, c);\n";
  out << "  });\n";
  out << "}\n";

  out << "template <typename T> dsl::Parser<std::string> lift(const dsl::Parser<T>& p) {\n";
  out << "  return dsl::parser([p](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{\n";
  out << "    auto r = p(in);\n";
  out << "    if (!r) return dsl::ExpectedResult<std::string>::failure(r.error.pos, r.error.kind, r.error.expected);\n";
  out << "    std::ostringstream ss;\n";
  out << "    if constexpr (std::is_same_v<T, std::string>) { ss << *r; }\n";
  out << "    else if constexpr (std::is_same_v<T, char>) { ss << *r; }\n";
  out << "    else if constexpr (std::is_same_v<T, std::vector<std::string>>) { for (const auto& it : *r) ss << it; }\n";
  out << "    else if constexpr (std::is_same_v<T, std::optional<std::string>>) { if (r->has_value()) ss << r->value(); }\n";
  out << "    else { ss << \"<node>\"; }\n";
  out << "    return ss.str();\n";
  out << "  });\n";
  out << "}\n";

  out << "dsl::Parser<std::string> seq(const dsl::Parser<std::string>& a, const dsl::Parser<std::string>& b) {\n";
  out << "  return dsl::parser([a, b](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{\n";
  out << "    auto r = (a & b)(in);\n";
  out << "    if (!r) return dsl::ExpectedResult<std::string>::failure(r.error.pos, r.error.kind, r.error.expected);\n";
  out << "    return (*r).first + (*r).second;\n";
  out << "  });\n";
  out << "}\n";

  out << "template <typename T> dsl::Parser<std::vector<T>> many1(const dsl::Parser<T>& p) {\n";
  out << "  return dsl::parser([p](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{\n";
  out << "    auto first = p(in);\n";
  out << "    if (!first) return dsl::ExpectedResult<std::vector<T>>::failure(first.error.pos, first.error.kind, first.error.expected);\n";
  out << "    std::vector<T> out;\n";
  out << "    out.push_back(*first);\n";
  out << "    while (true) {\n";
  out << "      auto next = p(in);\n";
  out << "      if (!next) break;\n";
  out << "      out.push_back(*next);\n";
  out << "    }\n";
  out << "    return out;\n";
  out << "  });\n";
  out << "}\n";

  out << "template <typename T, typename Open, typename Close> dsl::Parser<T> between(const dsl::Parser<Open>& open, const dsl::Parser<T>& core, const dsl::Parser<Close>& close) {\n";
  out << "  return dsl::parser([open, core, close](dsl::ParsecInput& in)->dsl::ExpectedResult<T>{\n";
  out << "    auto left = open(in); if (!left) return dsl::ExpectedResult<T>::failure(left.error.pos, left.error.kind, left.error.expected);\n";
  out << "    auto mid = core(in); if (!mid) return dsl::ExpectedResult<T>::failure(mid.error.pos, mid.error.kind, mid.error.expected);\n";
  out << "    auto right = close(in); if (!right) return dsl::ExpectedResult<T>::failure(right.error.pos, right.error.kind, right.error.expected);\n";
  out << "    return *mid;\n";
  out << "  });\n";
  out << "}\n";

  out << "template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep) {\n";
  out << "  return dsl::parser([item, sep](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{\n";
  out << "    auto save = in.pos;\n";
  out << "    auto one = sep_by1(item, sep)(in);\n";
  out << "    if (!one) {\n";
  out << "      in.pos = save;\n";
  out << "      return std::vector<T>{};\n";
  out << "    }\n";
  out << "    return *one;\n";
  out << "  });\n";
  out << "}\n";

  out << "template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by1(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep) {\n";
  out << "  return dsl::parser([item, sep](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{\n";
  out << "    std::vector<T> out;\n";
  out << "    auto first = item(in);\n";
  out << "    if (!first) return dsl::ExpectedResult<std::vector<T>>::failure(first.error.pos, first.error.kind, first.error.expected);\n";
  out << "    out.push_back(*first);\n";
  out << "    while (true) {\n";
  out << "      auto save = in.pos;\n";
  out << "      auto s = sep(in);\n";
  out << "      if (!s) { in.pos = save; break; }\n";
  out << "      auto v = item(in);\n";
  out << "      if (!v) { in.pos = save; break; }\n";
  out << "      out.push_back(*v);\n";
  out << "    }\n";
  out << "    return out;\n";
  out << "  });\n";
  out << "}\n";
  out << "}\n\n";

  out << "Parser::Parser(std::string source_name) { ctx_.file = std::move(source_name); }\n\n";

  out << "dsl::Parser<std::string> Parser::skip_ignored() {\n";
  out << "  return dsl::parser([this](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{\n";
  out << "    std::string out;\n";
  out << "    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) out.push_back(in.consume());\n";
  out << "    ctx_.chan = \"@IGNORE\";\n";
  out << "    return out;\n";
  out << "  });\n";
  out << "}\n\n";

  for (const auto& rule : grammar.rules) {
    const auto fn = map_rule_name_to_fn(rule.name);
    out << "dsl::Parser<std::string> Parser::parse_" << fn << "() {\n";
    out << "  return parzek_support::lift(" << expr_to_cpp(rule.expr, lexical_rules, !rule.lexical) << ");\n";
    out << "}\n\n";
  }

  std::string resolved_start = grammar.rules.front().name;
  auto first_nonterminal = std::find_if(grammar.rules.begin(), grammar.rules.end(), [](const Rule& rule) { return !rule.lexical; });
  if (first_nonterminal != grammar.rules.end()) {
    resolved_start = first_nonterminal->name;
  }
  if (grammar.parser_start.has_value() && !grammar.parser_start->empty()) {
    resolved_start = *grammar.parser_start;
  }
  const auto start_fn = map_rule_name_to_fn(resolved_start);
  out << "dsl::ParseOutcome<std::string> Parser::parse(std::string_view input) {\n";
  out << "  return dsl::run_parser(parse_" << start_fn << "(), input);\n";
  out << "}\n\n";
  
  out << "std::shared_ptr<ASTNode> Parser::parse_ast(std::string_view input) {\n";
  out << "  dsl::ParsecInput in(input);\n";
  out << "  active_input_ = &in;\n";
  out << "  auto result = parse_" << start_fn << "_ast();\n";
  out << "  active_input_ = nullptr;\n";
  out << "  return result;\n";
  out << "}\n\n";
  
  for (const auto& rule : grammar.rules) {
    if (!rule.lexical) {
      out << "std::shared_ptr<ASTNode> Parser::parse_" << map_rule_name_to_fn(rule.name) << "_ast() {\n";
      out << "  auto node = std::make_shared<ASTNode>();\n";
      out << "  node->rule_name = \"" << rule.name << "\";\n";
      out << "  node->line = ctx_.line;\n";
      out << "  node->column = ctx_.column;\n";
      out << "  auto result = parse_" << map_rule_name_to_fn(rule.name) << "()(*active_input_);\n";
      out << "  if (result) {\n";
      out << "    node->text = *result;\n";
      out << "  }\n";
      out << "  return node;\n";
      out << "}\n\n";
    }
  }

  out << "}\n";
  return out.str();
}

static std::string make_visitor_header(const Grammar& grammar, const std::string& visitor_name, const std::string& parser_header_name) {
  std::ostringstream out;
  out << "#pragma once\n\n";
  out << "#include \"" << parser_header_name << "\"\n\n";
  
  out << "namespace " << sanitize_symbol(grammar.parser_name) << "_parzek {\n\n";
  
  out << "struct " << sanitize_symbol(visitor_name) << " {\n";
  out << "  virtual ~" << sanitize_symbol(visitor_name) << "() = default;\n";
  out << "  virtual void visit(const ASTNode& node);\n";
  out << "  virtual void enter_node(const ASTNode& node) {}\n";
  out << "  virtual void exit_node(const ASTNode& node) {}\n";
  for (const auto& rule : grammar.rules) {
    if (!rule.lexical) {
      out << "  virtual void visit_" << map_rule_name_to_fn(rule.name) << "(const ASTNode& node) {}\n";
    }
  }
  out << "};\n";
  
  out << "\nvoid " << sanitize_symbol(visitor_name) << "::visit(const ASTNode& node) {\n";
  out << "  enter_node(node);\n";
  bool first = true;
  for (const auto& rule : grammar.rules) {
    if (!rule.lexical) {
      out << "  " << (first ? "" : "else ") << "if (node.rule_name == \"" << rule.name << "\") {\n";
      out << "    visit_" << map_rule_name_to_fn(rule.name) << "(node);\n";
      out << "  }\n";
      first = false;
    }
  }
  out << "  for (const auto& child : node.children) {\n";
  out << "    if (child) visit(*child);\n";
  out << "  }\n";
  out << "  exit_node(node);\n";
  out << "}\n\n";
  
  out << "}\n";
  return out.str();
}

static bool write_text_file(const std::filesystem::path& path, const std::string& content, CompileResult& result, const std::string& source_name) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    push_diag(result, "Cannot create output directory: " + path.parent_path().string(), source_name, 1, 1);
    return false;
  }
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    push_diag(result, "Cannot write file: " + path.string(), source_name, 1, 1);
    return false;
  }
  out << content;
  if (!out.good()) {
    push_diag(result, "Write failed for file: " + path.string(), source_name, 1, 1);
    return false;
  }
  return true;
}

static std::string basename_from_path(const std::string& path) {
  std::filesystem::path p(path);
  auto stem = p.stem().string();
  return stem.empty() ? "Generated" : stem;
}

static std::optional<std::string> load_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static bool has_required_parser_name(const std::string& source) {
  return source.find("@parser:name") != std::string::npos;
}

static std::string sanitize_output_basename(std::string value) {
  if (value.empty()) return "Generated";
  for (char& ch : value) {
    if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) ch = '_';
  }
  return value;
}

static dsl::Parser<std::string> combinator_identifier() {
  auto first = dsl::satisfy([](char c){ return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }, "ident-start");
  auto rest = *dsl::satisfy([](char c){ return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':'; }, "ident-char");
  return dsl::parser([first, rest](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    auto a = first(in);
    if (!a) return dsl::ExpectedResult<std::string>::failure(a.error.pos, a.error.kind, a.error.expected);
    auto b = rest(in);
    if (!b) return dsl::ExpectedResult<std::string>::failure(b.error.pos, b.error.kind, b.error.expected);
    std::string out(1, *a);
    for (char c : *b) out.push_back(c);
    return out;
  });
}

static bool smoke_parse_rule_signature(std::string_view src) {
  auto ident = combinator_identifier();
  auto colon = dsl::ch(':');
  auto semi = dsl::ch(';');
  auto literal = dsl::parser([](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    if (in.peek() != '"') return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Soft, {"string"});
    in.consume();
    std::string out;
    while (!in.eof() && in.peek() != '"') out.push_back(in.consume());
    if (in.eof()) return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Committed, {"closing quote"});
    in.consume();
    return out;
  });

  auto parser = dsl::labeled(dsl::try_parse(ident & colon & literal & semi), "rule-signature");
  auto outcome = dsl::run_parser(parser, src);
  return outcome.value.has_value();
}

}  // namespace

CompileResult compile_grammar_string(std::string_view grammar_source,
                                     const CompileOptions& options) {
  CompileResult result;
  std::string source_name = options.source_name;

  if (!has_required_parser_name(std::string(grammar_source))) {
    push_diag(result, "Missing mandatory @parser:name(...) directive", source_name, 1, 1);
    return result;
  }

  std::string preprocessed = preprocess(grammar_source, result, source_name);
  if (!result.diagnostics.empty()) return result;

  Tokenizer tokenizer(preprocessed);
  auto tokens = tokenizer.tokenize();

  GrammarParser parser(std::move(tokens), result, source_name);
  auto grammar = parser.parse();
  if (!grammar || !result.diagnostics.empty()) return result;
  if (grammar->parser_start.has_value()) {
    auto found = std::find_if(grammar->rules.begin(), grammar->rules.end(), [&](const Rule& rule) {
      return rule.name == *grammar->parser_start;
    });
    if (found == grammar->rules.end()) {
      push_diag(result, "Configured @parser:start rule not found: " + *grammar->parser_start, source_name, 1, 1);
      return result;
    }
  }

  std::filesystem::path out_dir = options.output_directory.empty() ? "." : options.output_directory;
  std::error_code mk_ec;
  std::filesystem::create_directories(out_dir, mk_ec);
  if (mk_ec) {
    push_diag(result, "Cannot create output directory: " + out_dir.string(), source_name, 1, 1);
    return result;
  }

  std::string basename = sanitize_output_basename(options.output_basename.value_or(grammar->parser_name));
  std::filesystem::path header_path = out_dir / (basename + "-Parzek.hpp");
  std::filesystem::path source_path = out_dir / (basename + "-Parzek.cpp");

  auto header = make_header(*grammar, basename + "-Parzek");
  auto source = make_source(*grammar, header_path.filename().string());

  if (!write_text_file(header_path, header, result, source_name)) return result;
  if (!write_text_file(source_path, source, result, source_name)) return result;

  result.parser_header_path = header_path.string();
  result.parser_source_path = source_path.string();

  if (options.visitor_header) {
    std::filesystem::path visitor_path = out_dir / *options.visitor_header;
    auto visitor = make_visitor_header(*grammar, std::filesystem::path(*options.visitor_header).stem().string(), header_path.filename().string());
    if (!write_text_file(visitor_path, visitor, result, source_name)) return result;
    result.visitor_header_path = visitor_path.string();
  }

  result.success = true;
  return result;
}

CompileResult compile_grammar_file(const std::string& grammar_path,
                                   const CompileOptions& options) {
  CompileOptions adjusted = options;
  adjusted.source_name = grammar_path;
  auto contents = load_file(grammar_path);
  if (!contents) {
    CompileResult result;
    push_diag(result, "Cannot open grammar file", grammar_path, 1, 1);
    return result;
  }
  if (!adjusted.output_basename) {
    adjusted.output_basename = basename_from_path(grammar_path);
  }
  return compile_grammar_string(*contents, adjusted);
}

int run_cli(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: parzek compile <grammar.pzg> [--create-visitor=Name.hpp] [--output-dir=DIR] [--output-base=NAME]\\n";
    return 1;
  }
  if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    std::cout << "Usage: parzek compile <grammar.pzg> [--create-visitor=Name.hpp] [--output-dir=DIR] [--output-base=NAME] [--stdin]\\n";
    return 0;
  }
  if (argc < 3) {
    std::cerr << "Usage: parzek compile <grammar.pzg> [--create-visitor=Name.hpp] [--output-dir=DIR] [--output-base=NAME]\\n";
    return 1;
  }

  std::string command = argv[1];
  if (command != "compile") {
    std::cerr << "Only 'compile' command is supported.\\n";
    return 1;
  }

  CompileOptions options;
  std::string input;
  bool from_stdin = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--create-visitor=", 0) == 0) {
      options.visitor_header = arg.substr(std::string("--create-visitor=").size());
    } else if (arg.rfind("--output-dir=", 0) == 0) {
      options.output_directory = arg.substr(std::string("--output-dir=").size());
    } else if (arg.rfind("--output-base=", 0) == 0) {
      options.output_basename = arg.substr(std::string("--output-base=").size());
    } else if (arg == "--stdin") {
      from_stdin = true;
    } else if (input.empty()) {
      input = arg;
    } else if (arg.rfind("--", 0) == 0) {
      std::cerr << "Unknown option: " << arg << "\\n";
      return 1;
    }
  }

  CompileResult result;
  if (from_stdin) {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    if (!options.output_basename) options.output_basename = "stdin";
    result = compile_grammar_string(ss.str(), options);
  } else {
    if (input.empty()) {
      std::cerr << "Missing input grammar path.\\n";
      return 1;
    }
    result = compile_grammar_file(input, options);
  }

  if (!result.success) {
    for (const auto& d : result.diagnostics) {
      std::cerr << d.file << ':' << d.line << ':' << d.column << ": " << d.message << '\n';
    }
    return 1;
  }

  std::cout << "Generated " << result.parser_header_path << " and " << result.parser_source_path << '\n';
  if (result.visitor_header_path) {
    std::cout << "Generated visitor " << *result.visitor_header_path << '\n';
  }
  return 0;
}

}  // namespace parzek
