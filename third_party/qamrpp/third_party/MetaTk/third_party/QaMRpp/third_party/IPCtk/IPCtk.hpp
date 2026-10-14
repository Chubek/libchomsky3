#ifndef IPCTK_HPP
#define IPCTK_HPP

#if __has_include("Polyflow/Polyflow.hpp") && __has_include("Polyflow/Grafitt/DSLUtils.hpp")
#include "Polyflow/Grafitt/DSLUtils.hpp"
#include "Polyflow/Polyflow.hpp"
#define IPCTK_HAS_POLYFLOW 1
#else
#include "DSLUtils.hpp"
#define IPCTK_HAS_POLYFLOW 0
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ipctk {

struct Error {
  enum class Code { Parse, Validation, Lowering, Emission, Runtime, Unsupported };
  Code code{Code::Runtime};
  std::string message{};
  std::size_t position{0};
};

template <typename T> using Result = dsl::Result<T, Error>;
inline auto make_error(Error::Code c, std::string m, std::size_t p = 0) -> Error { return Error{c, std::move(m), p}; }

namespace ir {
struct Resource { std::string kind{}, name{}, initializer{}; };
struct Step;
struct Arg { std::string value{}; std::optional<std::shared_ptr<Step>> nested{}; };
struct Step { std::string op{}; std::vector<Arg> args{}; };
struct Pipe { std::string name{}; std::vector<Step> steps{}; };
struct Program { std::vector<Resource> resources{}; std::vector<Pipe> pipes{}; dsl::ASTNode ast{}; };
struct BackendRule { std::string operation{}, emit{}; dsl::ASTNode ast{}; };
struct BackendSpec { std::string target{}; std::set<std::string> capabilities{}; std::vector<BackendRule> rules{}; dsl::ASTNode ast{}; };
} // namespace ir

namespace parse {
inline auto trim(std::string_view sv) -> std::string_view {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
  return sv;
}

inline auto strip_comments(std::string_view src) -> std::string {
  std::string out;
  bool line = false;
  bool block = false;
  bool in_single = false;
  bool in_double = false;
  bool escaped = false;
  for (std::size_t i = 0; i < src.size(); ++i) {
    const char c = src[i];
    const char n = (i + 1 < src.size()) ? src[i + 1] : '\0';

    if (line) {
      if (c == '\n') {
        line = false;
        out.push_back(c);
      }
      continue;
    }
    if (block) {
      if (c == '*' && n == '/') {
        block = false;
        ++i;
      }
      continue;
    }

    if ((in_single || in_double) && !escaped && c == '\\') {
      escaped = true;
      out.push_back(c);
      continue;
    }

    if (!escaped) {
      if (!in_double && c == '\'') {
        in_single = !in_single;
        out.push_back(c);
        continue;
      }
      if (!in_single && c == '"') {
        in_double = !in_double;
        out.push_back(c);
        continue;
      }
    }

    if (!in_single && !in_double && c == '/' && n == '/') {
      line = true;
      ++i;
      continue;
    }
    if (!in_single && !in_double && c == '/' && n == '*') {
      block = true;
      ++i;
      continue;
    }

    out.push_back(c);
    escaped = false;
  }
  return out;
}

inline auto ws() {
  return dsl::parser([](dsl::ParsecInput& in)->dsl::ExpectedResult<char>{
    while (!in.eof() && (in.peek() == ' ' || in.peek() == '\t' || in.peek() == '\n' || in.peek() == '\r')) in.consume();
    return '\0';
  });
}

inline auto ident() {
  return dsl::parser([](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    while (!in.eof() && (in.peek() == ' ' || in.peek() == '\t' || in.peek() == '\n' || in.peek() == '\r')) in.consume();
    if (in.eof() || !(std::isalpha(static_cast<unsigned char>(in.peek())) || in.peek() == '_')) return dsl::fail_expected<std::string>(in, "identifier");
    std::string out;
    out.push_back(in.consume());
    while (!in.eof()) {
      const char c = in.peek();
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-')) break;
      out.push_back(in.consume());
    }
    return out;
  });
}

inline auto token(char c) {
  return dsl::parser([c](dsl::ParsecInput& in)->dsl::ExpectedResult<char>{
    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) in.consume();
    if (!in.eof() && in.peek() == c) return in.consume();
    return dsl::fail_expected<char>(in, std::string(1, c));
  });
}

inline auto until(char endc) {
  return dsl::parser([endc](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    std::string out;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    while (!in.eof()) {
      const char c = in.peek();
      if (!escaped && c == '\\' && (in_single || in_double)) {
        out.push_back(in.consume());
        escaped = true;
        continue;
      }
      if (!escaped && !in_double && c == '\'') {
        in_single = !in_single;
        out.push_back(in.consume());
        continue;
      }
      if (!escaped && !in_single && c == '"') {
        in_double = !in_double;
        out.push_back(in.consume());
        continue;
      }

      if (!in_single && !in_double) {
        if (c == '(') ++paren;
        else if (c == ')') {
          if (paren == 0) return dsl::fail_expected<std::string>(in, "balanced-parenthesis", dsl::ParseFailureKind::Committed);
          --paren;
        }
        else if (c == '{') ++brace;
        else if (c == '}') {
          if (brace == 0) return dsl::fail_expected<std::string>(in, "balanced-brace", dsl::ParseFailureKind::Committed);
          --brace;
        }
        else if (c == '[') ++bracket;
        else if (c == ']') {
          if (bracket == 0) return dsl::fail_expected<std::string>(in, "balanced-bracket", dsl::ParseFailureKind::Committed);
          --bracket;
        }
        else if (c == endc && paren == 0 && brace == 0 && bracket == 0) {
          break;
        }
      }

      out.push_back(in.consume());
      escaped = false;
    }

    if (in_single || in_double) return dsl::fail_expected<std::string>(in, "closing-quote", dsl::ParseFailureKind::Committed);
    if (paren != 0 || brace != 0 || bracket != 0) return dsl::fail_expected<std::string>(in, "balanced-delimiters", dsl::ParseFailureKind::Committed);
    return out;
  });
}

inline auto read_rule_body(dsl::ParsecInput& in) -> dsl::ExpectedResult<std::string> {
  while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) {
    if (in.peek() == '\n') break;
    in.consume();
  }

  if (in.eof()) return dsl::fail_expected<std::string>(in, "rule-body");

  if (in.peek() == '"' && in.pos + 2 < in.source.size() && in.source[in.pos + 1] == '"' && in.source[in.pos + 2] == '"') {
    in.consume();
    in.consume();
    in.consume();
    std::string out;
    while (!in.eof()) {
      if (in.peek() == '"' && in.pos + 2 < in.source.size() && in.source[in.pos + 1] == '"' && in.source[in.pos + 2] == '"') {
        in.consume();
        in.consume();
        in.consume();
        return out;
      }
      out.push_back(in.consume());
    }
    return dsl::fail_expected<std::string>(in, "closing-triple-quote", dsl::ParseFailureKind::Committed);
  }

  if (in.peek() == '{') {
    in.consume();
    std::string out;
    int depth = 1;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    while (!in.eof()) {
      const char c = in.consume();

      if (!escaped && c == '\\' && (in_single || in_double)) {
        out.push_back(c);
        escaped = true;
        continue;
      }
      if (!escaped && !in_double && c == '\'') {
        in_single = !in_single;
        out.push_back(c);
        continue;
      }
      if (!escaped && !in_single && c == '"') {
        in_double = !in_double;
        out.push_back(c);
        continue;
      }

      if (!in_single && !in_double) {
        if (c == '{') ++depth;
        if (c == '}') {
          --depth;
          if (depth == 0) return out;
        }
      }

      if (depth > 0) out.push_back(c);
      escaped = false;
    }

    return dsl::fail_expected<std::string>(in, "closing-brace", dsl::ParseFailureKind::Committed);
  }

  auto line = until('\n')(in);
  if (!line) return line;
  return std::string(trim(*line));
}

inline auto split_top_level(std::string_view src, std::string_view delim) -> std::vector<std::string> {
  std::vector<std::string> out;
  if (delim.empty()) {
    out.emplace_back(std::string(trim(src)));
    return out;
  }

  int paren = 0;
  int brace = 0;
  int bracket = 0;
  bool in_single = false;
  bool in_double = false;
  bool escaped = false;
  std::size_t start = 0;

  for (std::size_t i = 0; i < src.size(); ++i) {
    const char c = src[i];

    if (!escaped && c == '\\' && (in_single || in_double)) {
      escaped = true;
      continue;
    }
    if (!escaped && !in_double && c == '\'') {
      in_single = !in_single;
      continue;
    }
    if (!escaped && !in_single && c == '"') {
      in_double = !in_double;
      continue;
    }

    if (!in_single && !in_double) {
      if (c == '(') ++paren;
      else if (c == ')' && paren > 0) --paren;
      else if (c == '{') ++brace;
      else if (c == '}' && brace > 0) --brace;
      else if (c == '[') ++bracket;
      else if (c == ']' && bracket > 0) --bracket;

      if (paren == 0 && brace == 0 && bracket == 0 && i + delim.size() <= src.size() && src.substr(i, delim.size()) == delim) {
        out.emplace_back(std::string(trim(src.substr(start, i - start))));
        i += delim.size() - 1;
        start = i + 1;
      }
    }
    escaped = false;
  }

  out.emplace_back(std::string(trim(src.substr(start))));
  return out;
}
} // namespace parse

namespace detail {
inline auto ast_list(std::string_view tag, std::vector<dsl::ASTNode> children) -> dsl::ASTNode {
  return dsl::ASTNode(tag, std::move(children));
}
} // namespace detail

namespace ipcl {
struct NativeDSL : dsl::DSL<NativeDSL, dsl::Pipeline, dsl::Rewrite, dsl::PatternMatch, dsl::Operators, dsl::CombinatorParser, dsl::AST> {};

template <dsl::FixedString Name> struct message_type {};
template <typename T> struct as_t { using type = T; };
template <typename T> inline constexpr as_t<T> as{};

struct ResourceBuilder { std::string kind{}, name{}; auto operator=(std::string init) const -> ir::Resource { return {kind, name, std::move(init)}; } };
struct StepExpr { ir::Step step{}; };
struct PipeExpr { std::vector<ir::Step> steps{}; };
struct PipeBuilder { std::string name{}; auto operator=(PipeExpr p) const -> ir::Pipe { return {name, std::move(p.steps)}; } };
inline auto operator>>(StepExpr a, StepExpr b)->PipeExpr { return {{std::move(a.step), std::move(b.step)}}; }
inline auto operator>>(PipeExpr a, StepExpr b)->PipeExpr { a.steps.push_back(std::move(b.step)); return a; }
inline auto socket(std::string n)->ResourceBuilder{ return {"socket",std::move(n)}; }
inline auto queue(std::string n)->ResourceBuilder{ return {"queue",std::move(n)}; }
inline auto mutex(std::string n)->ResourceBuilder{ return {"mutex",std::move(n)}; }
inline auto signal(std::string n)->ResourceBuilder{ return {"signal",std::move(n)}; }
inline auto shared(std::string n)->ResourceBuilder{ return {"shared",std::move(n)}; }
inline auto pipe(std::string n)->PipeBuilder{ return {std::move(n)}; }
inline auto mk(std::string op, std::vector<ir::Arg> a={}) -> StepExpr { return {ir::Step{std::move(op), std::move(a)}}; }
inline auto recv(const ir::Resource& r)->StepExpr{ return mk("recv",{{r.name,std::nullopt}}); }
inline auto send(const ir::Resource& r)->StepExpr{ return mk("send",{{r.name,std::nullopt}}); }
inline auto enqueue(const ir::Resource& r)->StepExpr{ return mk("enqueue",{{r.name,std::nullopt}}); }
inline auto dequeue(const ir::Resource& r)->StepExpr{ return mk("dequeue",{{r.name,std::nullopt}}); }
inline auto lock(const ir::Resource& r)->StepExpr{ return mk("lock",{{r.name,std::nullopt}}); }
inline auto unlock(const ir::Resource& r)->StepExpr{ return mk("unlock",{{r.name,std::nullopt}}); }
inline auto notify(const ir::Resource& r)->StepExpr{ return mk("notify",{{r.name,std::nullopt}}); }
inline auto wait(const ir::Resource& r)->StepExpr{ return mk("wait",{{r.name,std::nullopt}}); }
template <typename T> inline auto decode(as_t<T>)->StepExpr { return mk("decode", {{std::string(T::value), std::nullopt}}); }
template <typename T> inline auto encode(as_t<T>)->StepExpr { return mk("encode", {{std::string(T::value), std::nullopt}}); }

inline auto parse_step(std::string_view src) -> std::optional<ir::Step> {
  src = parse::trim(src);
  if (src.empty()) return std::nullopt;

  auto open = src.find('(');
  auto close = src.rfind(')');
  if (open == std::string_view::npos || close == std::string_view::npos || close <= open) return std::nullopt;
  if (!parse::trim(src.substr(close + 1)).empty()) return std::nullopt;

  const auto op_sv = parse::trim(src.substr(0, open));
  if (op_sv.empty()) return std::nullopt;

  ir::Step out{};
  out.op = std::string(op_sv);

  const auto inside = src.substr(open + 1, close - open - 1);
  if (parse::trim(inside).empty()) return out;

  for (const auto& tok : parse::split_top_level(inside, ",")) {
    const auto t = parse::trim(tok);
    if (t.empty()) return std::nullopt;
    if (auto nested = parse_step(t)) {
      out.args.push_back(ir::Arg{"", std::make_shared<ir::Step>(std::move(*nested))});
    } else {
      out.args.push_back(ir::Arg{std::string(t), std::nullopt});
    }
  }
  return out;
}

inline auto parse_text(std::string_view source) -> Result<ir::Program> {
  using namespace parse;
  const auto clean = strip_comments(source);
  auto p = dsl::parser([&](dsl::ParsecInput& in)->dsl::ExpectedResult<ir::Program>{
    ir::Program out{};
    while (!in.eof()) {
      ws()(in);
      if (in.eof()) break;

      auto kw = ident()(in);
      if (!kw) return dsl::ExpectedResult<ir::Program>::failure(kw.error.pos, kw.error.kind, kw.error.expected);

      if (*kw == "pipe") {
        auto nm = ident()(in);
        if (!nm) return dsl::ExpectedResult<ir::Program>::failure(nm.error.pos, nm.error.kind, nm.error.expected);
        if (!token('=')(in)) return dsl::fail_expected<ir::Program>(in, "=", dsl::ParseFailureKind::Committed);
        auto rhs = until(';')(in);
        if (!rhs) return dsl::ExpectedResult<ir::Program>::failure(rhs.error.pos, rhs.error.kind, rhs.error.expected);
        if (!token(';')(in)) return dsl::fail_expected<ir::Program>(in, ";", dsl::ParseFailureKind::Committed);

        ir::Pipe pp{*nm, {}};
        for (const auto& part : split_top_level(*rhs, "->")) {
          if (part.empty()) continue;
          auto st = parse_step(part);
          if (!st) return dsl::fail_expected<ir::Program>(in, "pipe-step", dsl::ParseFailureKind::Committed);
          pp.steps.push_back(std::move(*st));
        }
        if (pp.steps.empty()) return dsl::fail_expected<ir::Program>(in, "non-empty-pipe", dsl::ParseFailureKind::Committed);
        out.pipes.push_back(std::move(pp));
      } else {
        auto nm = ident()(in);
        if (!nm) return dsl::ExpectedResult<ir::Program>::failure(nm.error.pos, nm.error.kind, nm.error.expected);
        if (!token('=')(in)) return dsl::fail_expected<ir::Program>(in, "=", dsl::ParseFailureKind::Committed);
        auto init = until(';')(in);
        if (!init) return dsl::ExpectedResult<ir::Program>::failure(init.error.pos, init.error.kind, init.error.expected);
        if (!token(';')(in)) return dsl::fail_expected<ir::Program>(in, ";", dsl::ParseFailureKind::Committed);
        out.resources.push_back(ir::Resource{*kw, *nm, std::string(trim(*init))});
      }
    }

    std::vector<dsl::ASTNode> res_nodes;
    std::vector<dsl::ASTNode> pipe_nodes;
    for (const auto& r : out.resources) res_nodes.push_back(dsl::node<"resource">(dsl::leaf<"kind">(r.kind), dsl::leaf<"name">(r.name)));
    for (const auto& pp : out.pipes) pipe_nodes.push_back(dsl::node<"pipe">(dsl::leaf<"name">(pp.name), dsl::leaf<"steps">((int)pp.steps.size())));

    out.ast = dsl::node<"ipcl.program">(
        detail::ast_list("resources", std::move(res_nodes)),
        detail::ast_list("pipes", std::move(pipe_nodes)),
        dsl::leaf<"resource_count">((int)out.resources.size()),
        dsl::leaf<"pipe_count">((int)out.pipes.size()));
    return out;
  });

  auto res = dsl::run_parser(p, clean);
  if (!res.value) {
    return Result<ir::Program>::from_err(make_error(
        Error::Code::Parse,
        "ipcl parse failed, expected: " + (res.error.expected.empty() ? std::string("token") : res.error.expected.front()),
        res.error.pos));
  }
  return Result<ir::Program>::from_ok(*res.value);
}
} // namespace ipcl

namespace itkd {
struct NativeDSL : dsl::DSL<NativeDSL, dsl::Pipeline, dsl::Rewrite, dsl::PatternMatch, dsl::Operators, dsl::CombinatorParser, dsl::AST> {};

struct BackendBuilder {
  ir::BackendSpec spec{};
  auto target(std::string v)->BackendBuilder&{ spec.target = std::move(v); return *this; }
  auto capability(std::string v)->BackendBuilder&{ spec.capabilities.insert(std::move(v)); return *this; }
  auto rule(std::string op, std::string emit)->BackendBuilder&{
    spec.rules.push_back({std::move(op), std::move(emit), dsl::node<"rule">()});
    return *this;
  }
  auto build()->ir::BackendSpec{
    std::vector<dsl::ASTNode> cap_nodes;
    std::vector<dsl::ASTNode> rule_nodes;
    for (const auto& cap : spec.capabilities) cap_nodes.push_back(dsl::leaf<"capability">(cap));
    for (const auto& r : spec.rules) rule_nodes.push_back(dsl::node<"rule">(dsl::leaf<"op">(r.operation), dsl::leaf<"emit">(r.emit)));
    spec.ast = dsl::node<"itkd.backend">(
        dsl::leaf<"target">(spec.target),
        detail::ast_list("capabilities", std::move(cap_nodes)),
        detail::ast_list("rules", std::move(rule_nodes)));
    return spec;
  }
};

inline auto backend()->BackendBuilder { return {}; }

inline auto parse_text(std::string_view source)->Result<ir::BackendSpec> {
  using namespace parse;
  const auto clean = strip_comments(source);

  auto parse_line_tail = [&](dsl::ParsecInput& in) -> dsl::ExpectedResult<std::string> {
    auto body = until('\n')(in);
    if (!body) return body;
    auto text = std::string(trim(*body));
    if (!text.empty() && text.back() == ';') {
      text.pop_back();
      text = std::string(trim(text));
    }
    return text;
  };

  auto p = dsl::parser([&](dsl::ParsecInput& in)->dsl::ExpectedResult<ir::BackendSpec>{
    ir::BackendSpec spec{};
    std::vector<dsl::ASTNode> imports{};
    std::vector<dsl::ASTNode> includes{};
    std::vector<dsl::ASTNode> maps{};

    while (!in.eof()) {
      ws()(in);
      if (in.eof()) break;

      auto kw = ident()(in);
      if (!kw) return dsl::ExpectedResult<ir::BackendSpec>::failure(kw.error.pos, kw.error.kind, kw.error.expected);

      if (*kw == "target") {
        auto id = ident()(in);
        if (!id) return dsl::ExpectedResult<ir::BackendSpec>::failure(id.error.pos, id.error.kind, id.error.expected);
        spec.target = *id;
        auto tail = parse_line_tail(in);
        if (!tail) return dsl::ExpectedResult<ir::BackendSpec>::failure(tail.error.pos, tail.error.kind, tail.error.expected);
      } else if (*kw == "capability") {
        auto id = ident()(in);
        if (!id) return dsl::ExpectedResult<ir::BackendSpec>::failure(id.error.pos, id.error.kind, id.error.expected);
        spec.capabilities.insert(*id);
        auto tail = parse_line_tail(in);
        if (!tail) return dsl::ExpectedResult<ir::BackendSpec>::failure(tail.error.pos, tail.error.kind, tail.error.expected);
      } else if (*kw == "rule") {
        auto op = ident()(in);
        if (!op) return dsl::ExpectedResult<ir::BackendSpec>::failure(op.error.pos, op.error.kind, op.error.expected);
        if (!token('=')(in)) return dsl::fail_expected<ir::BackendSpec>(in, "=", dsl::ParseFailureKind::Committed);
        auto body = read_rule_body(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
        spec.rules.push_back({*op, std::string(trim(*body)), dsl::node<"rule">(dsl::leaf<"op">(*op), dsl::leaf<"emit">(std::string(trim(*body))))});
        while (!in.eof() && in.peek() != '\n') in.consume();
      } else if (*kw == "import") {
        auto body = parse_line_tail(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
        if (trim(*body).empty()) return dsl::fail_expected<ir::BackendSpec>(in, "import-path", dsl::ParseFailureKind::Committed);
        imports.push_back(dsl::leaf<"import">(*body));
      } else if (*kw == "include") {
        auto body = parse_line_tail(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
        if (trim(*body).empty()) return dsl::fail_expected<ir::BackendSpec>(in, "include-path", dsl::ParseFailureKind::Committed);
        includes.push_back(dsl::leaf<"include">(*body));
      } else if (*kw == "map") {
        auto body = parse_line_tail(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
        if (trim(*body).empty()) return dsl::fail_expected<ir::BackendSpec>(in, "map-body", dsl::ParseFailureKind::Committed);
        maps.push_back(dsl::leaf<"map">(*body));
      } else {
        return dsl::fail_expected<ir::BackendSpec>(in, "target/capability/rule/import/include/map", dsl::ParseFailureKind::Committed);
      }

      if (!in.eof() && in.peek() == '\n') in.consume();
    }

    std::vector<dsl::ASTNode> cap_nodes;
    std::vector<dsl::ASTNode> rule_nodes;
    for (const auto& cap : spec.capabilities) cap_nodes.push_back(dsl::leaf<"capability">(cap));
    for (const auto& r : spec.rules) rule_nodes.push_back(dsl::node<"rule">(dsl::leaf<"op">(r.operation), dsl::leaf<"emit">(r.emit)));

    spec.ast = dsl::node<"itkd.spec">(
        dsl::leaf<"target">(spec.target),
        detail::ast_list("capabilities", std::move(cap_nodes)),
        detail::ast_list("rules", std::move(rule_nodes)),
        detail::ast_list("imports", std::move(imports)),
        detail::ast_list("includes", std::move(includes)),
        detail::ast_list("maps", std::move(maps)));
    return spec;
  });

  auto o = dsl::run_parser(p, clean);
  if (!o.value) {
    return Result<ir::BackendSpec>::from_err(make_error(
        Error::Code::Parse,
        "itkd parse failed, expected: " + (o.error.expected.empty() ? std::string("token") : o.error.expected.front()),
        o.error.pos));
  }
  return Result<ir::BackendSpec>::from_ok(*o.value);
}
} // namespace itkd

namespace runtime {
class FdHandle {
 public:
  FdHandle() = default;
  explicit FdHandle(int fd):fd_(fd){}
  ~FdHandle(){ if(fd_>=0) ::close(fd_); }
  FdHandle(FdHandle&& o) noexcept : fd_(std::exchange(o.fd_,-1)) {}
  auto operator=(FdHandle&& o) noexcept -> FdHandle& {
    if(this!=&o){
      if(fd_>=0) ::close(fd_);
      fd_=std::exchange(o.fd_,-1);
    }
    return *this;
  }
  FdHandle(const FdHandle&) = delete;
  auto operator=(const FdHandle&) -> FdHandle& = delete;
  auto get() const -> int { return fd_; }
 private:
  int fd_{-1};
};

struct PipePair { FdHandle read_end{}, write_end{}; };

inline auto open_pipe() -> Result<PipePair> {
  int fds[2];
  if(::pipe(fds) != 0) return Result<PipePair>::from_err(make_error(Error::Code::Runtime,"pipe failed"));
  return Result<PipePair>::from_ok(PipePair{FdHandle{fds[0]}, FdHandle{fds[1]}});
}

class LocalSocket {
 public:
  static auto connect(std::string path) -> Result<LocalSocket> {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return Result<LocalSocket>::from_err(make_error(Error::Code::Runtime, "socket failed"));

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      ::close(fd);
      return Result<LocalSocket>::from_err(make_error(Error::Code::Runtime, "connect failed"));
    }
    return Result<LocalSocket>::from_ok(LocalSocket(FdHandle{fd}));
  }

  auto send(std::span<const std::byte> b)->Result<std::size_t>{
    auto n=::write(fd_.get(), b.data(), b.size());
    if(n<0) return Result<std::size_t>::from_err(make_error(Error::Code::Runtime,"write failed"));
    return Result<std::size_t>::from_ok((std::size_t)n);
  }

 private:
  explicit LocalSocket(FdHandle fd):fd_(std::move(fd)){}
  FdHandle fd_{};
};
} // namespace runtime

namespace detail {
inline auto is_builtin_op(std::string_view op) -> bool {
  static const std::set<std::string> kOps{
      "recv", "send", "enqueue", "dequeue", "lock", "unlock", "notify", "wait", "encode", "decode"};
  return kOps.find(std::string(op)) != kOps.end();
}

inline auto op_capability(std::string_view op) -> std::optional<std::string_view> {
  if (op == "recv" || op == "send") return "socket";
  if (op == "enqueue" || op == "dequeue") return "pubsub";
  if (op == "lock" || op == "unlock") return "sync";
  if (op == "notify" || op == "wait") return "signal";
  if (op == "encode" || op == "decode") return "codec";
  return std::nullopt;
}

inline auto kind_for_operation(std::string_view op) -> std::optional<std::string_view> {
  if (op == "recv" || op == "send") return "socket";
  if (op == "enqueue" || op == "dequeue") return "queue";
  if (op == "lock" || op == "unlock") return "mutex";
  if (op == "notify" || op == "wait") return "signal";
  return std::nullopt;
}

inline auto needs_resource_arg(std::string_view op) -> bool {
  return op == "recv" || op == "send" || op == "enqueue" || op == "dequeue" || op == "lock" || op == "unlock" || op == "notify" || op == "wait";
}

inline auto has_rule_for(const ir::BackendSpec& spec, std::string_view op) -> bool {
  for (const auto& rule : spec.rules) {
    if (rule.operation == op) return true;
  }
  return false;
}

inline auto step_uses_resource(const ir::Step& st, std::string& out_name) -> bool {
  if (st.args.empty()) return false;
  const auto& first = st.args.front();
  if (first.nested.has_value()) return false;
  if (first.value.empty()) return false;
  out_name = first.value;
  return true;
}

inline auto step_to_text(const ir::Step& st) -> std::string {
  std::string out = st.op + "(";
  for (std::size_t i = 0; i < st.args.size(); ++i) {
    if (i) out += ", ";
    if (st.args[i].nested.has_value()) out += step_to_text(*(*st.args[i].nested));
    else out += st.args[i].value;
  }
  out += ")";
  return out;
}

inline auto rule_map(const ir::BackendSpec& spec) -> std::map<std::string, std::string> {
  std::map<std::string, std::string> by_op;
  for (const auto& r : spec.rules) {
    if (!r.operation.empty()) by_op[r.operation] = r.emit;
  }
  return by_op;
}

inline auto replace_all(std::string text, std::string_view needle, std::string_view repl) -> std::string {
  if (needle.empty()) return text;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    text.replace(pos, needle.size(), repl);
    pos += repl.size();
  }
  return text;
}

inline auto render_emit_template(const std::string& tmpl, const ir::Step& st) -> std::string {
  std::string args_joined;
  for (std::size_t i = 0; i < st.args.size(); ++i) {
    if (i) args_joined += ", ";
    if (st.args[i].nested.has_value()) args_joined += step_to_text(*(*st.args[i].nested));
    else args_joined += st.args[i].value;
  }

  std::string out = tmpl;
  out = replace_all(std::move(out), "${op}", st.op);
  out = replace_all(std::move(out), "${step}", step_to_text(st));
  out = replace_all(std::move(out), "${args}", args_joined);

  for (std::size_t i = 0; i < st.args.size(); ++i) {
    const auto key = std::string("${arg") + std::to_string(i) + "}";
    const auto value = st.args[i].nested.has_value() ? step_to_text(*(*st.args[i].nested)) : st.args[i].value;
    out = replace_all(std::move(out), key, value);
  }

  if (out.empty() || out.back() != '\n') out.push_back('\n');
  return out;
}

inline auto emit_arg(const ir::Arg& arg) -> std::string {
  if (arg.nested.has_value()) return step_to_text(*(*arg.nested));
  return arg.value;
}

inline auto emit_default_step(const ir::Step& st) -> std::string {
  auto arg0 = [&]() -> std::string { return st.args.empty() ? std::string{} : emit_arg(st.args.front()); };

  if (st.op == "recv") return "ipc.recv(" + arg0() + ");\n";
  if (st.op == "send") return "ipc.send(" + arg0() + ");\n";
  if (st.op == "enqueue") return "ipc.enqueue(" + arg0() + ");\n";
  if (st.op == "dequeue") return "ipc.dequeue(" + arg0() + ");\n";
  if (st.op == "lock") return "ipc.lock(" + arg0() + ");\n";
  if (st.op == "unlock") return "ipc.unlock(" + arg0() + ");\n";
  if (st.op == "notify") return "ipc.notify(" + arg0() + ");\n";
  if (st.op == "wait") return "ipc.wait(" + arg0() + ");\n";
  if (st.op == "encode") return "ipc.encode(" + arg0() + ");\n";
  if (st.op == "decode") return "ipc.decode(" + arg0() + ");\n";

  std::string out = st.op + "(";
  for (std::size_t i = 0; i < st.args.size(); ++i) {
    if (i) out += ", ";
    out += emit_arg(st.args[i]);
  }
  out += ");\n";
  return out;
}

inline auto value_to_string(const std::string& fallback,
#if IPCTK_HAS_POLYFLOW
                            const zethamem::Value& value
#else
                            const std::string& value
#endif
                            ) -> std::string {
#if IPCTK_HAS_POLYFLOW
  if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
  if (std::holds_alternative<std::int64_t>(value)) return std::to_string(std::get<std::int64_t>(value));
  if (std::holds_alternative<double>(value)) {
    std::ostringstream oss;
    oss << std::get<double>(value);
    return oss.str();
  }
  if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
  return fallback;
#else
  return value;
#endif
}

struct LoweredProgram {
  std::vector<std::vector<std::string>> emitted{};
  std::size_t worker_count{1};
};

inline auto lower_program_serial(const ir::Program& p, const std::map<std::string, std::string>& by_op) -> LoweredProgram {
  LoweredProgram lower{};
  lower.worker_count = 1;
  lower.emitted.resize(p.pipes.size());
  for (std::size_t pi = 0; pi < p.pipes.size(); ++pi) {
    for (const auto& st : p.pipes[pi].steps) {
      auto it = by_op.find(st.op);
      if (it != by_op.end()) lower.emitted[pi].push_back(render_emit_template(it->second, st));
      else lower.emitted[pi].push_back(emit_default_step(st));
    }
  }
  return lower;
}

inline auto lower_program_polyflow(const ir::Program& p, const std::map<std::string, std::string>& by_op) -> Result<LoweredProgram> {
#if IPCTK_HAS_POLYFLOW
  struct Node {
    std::size_t id{};
    std::size_t pipe_idx{};
    std::size_t step_idx{};
    std::vector<std::size_t> deps{};
  };

  std::vector<Node> nodes{};
  nodes.reserve(128);

  std::unordered_map<std::string, std::size_t> last_resource_use;

  for (std::size_t pi = 0; pi < p.pipes.size(); ++pi) {
    std::optional<std::size_t> prev_in_pipe{};
    for (std::size_t si = 0; si < p.pipes[pi].steps.size(); ++si) {
      Node n{};
      n.id = nodes.size() + 1;
      n.pipe_idx = pi;
      n.step_idx = si;

      if (prev_in_pipe.has_value()) n.deps.push_back(*prev_in_pipe);

      std::string res_name;
      if (step_uses_resource(p.pipes[pi].steps[si], res_name)) {
        auto it = last_resource_use.find(res_name);
        if (it != last_resource_use.end()) n.deps.push_back(it->second);
        last_resource_use[res_name] = n.id;
      }

      std::sort(n.deps.begin(), n.deps.end());
      n.deps.erase(std::unique(n.deps.begin(), n.deps.end()), n.deps.end());

      nodes.push_back(n);
      prev_in_pipe = n.id;
    }
  }

  polyflow::task_graph graph;
  std::unordered_map<std::size_t, std::shared_ptr<polyflow::task_base>> tasks;
  tasks.reserve(nodes.size());

  for (const auto& node : nodes) {
    const auto& st = p.pipes[node.pipe_idx].steps[node.step_idx];
    const auto it = by_op.find(st.op);
    const std::string resolved = (it != by_op.end()) ? render_emit_template(it->second, st) : emit_default_step(st);

    polyflow::task_metadata md{};
    md.name = p.pipes[node.pipe_idx].name + ":" + std::to_string(node.step_idx);
    md.prio = polyflow::priority::normal;

    auto fn = [resolved](polyflow::cancellation_token&) -> zethamem::Value {
      return zethamem::Value{resolved};
    };

    auto task = std::make_shared<polyflow::task_base>(node.id, std::move(md), std::move(fn));
    for (const auto dep : node.deps) task->add_dependency(dep);
    tasks[node.id] = task;
    graph.add_task(task);
  }

  try {
    polyflow::execution_context exec(std::move(graph));
    exec.run_all();

    LoweredProgram out{};
    out.worker_count = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    out.emitted.resize(p.pipes.size());

    for (const auto& node : nodes) {
      const auto value = exec.wait(node.id);
      out.emitted[node.pipe_idx].push_back(value_to_string("", value));
    }

    return Result<LoweredProgram>::from_ok(std::move(out));
  } catch (const std::exception& ex) {
    return Result<LoweredProgram>::from_err(make_error(Error::Code::Lowering, std::string("polyflow lowering failed: ") + ex.what()));
  }
#else
  (void)p;
  (void)by_op;
  return Result<LoweredProgram>::from_err(make_error(Error::Code::Unsupported, "polyflow integration unavailable"));
#endif
}

inline auto lower_program(const ir::Program& p, const ir::BackendSpec& spec) -> Result<LoweredProgram> {
  const auto by_op = rule_map(spec);

#if IPCTK_HAS_POLYFLOW
  auto lowered = lower_program_polyflow(p, by_op);
  if (lowered.is_ok()) return lowered;
  if (lowered.is_err()) return Result<LoweredProgram>::from_ok(lower_program_serial(p, by_op));
#endif

  return Result<LoweredProgram>::from_ok(lower_program_serial(p, by_op));
}

inline auto validate_step_semantics(const ir::Step& st,
                                    const std::unordered_map<std::string, std::string>& resources,
                                    std::unordered_set<const ir::Step*>& visited,
                                    std::string_view where) -> Result<bool> {
  if (visited.find(&st) != visited.end()) return Result<bool>::from_err(make_error(Error::Code::Validation, "cyclic nested step", 0));
  visited.insert(&st);

  if (st.op.empty()) return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": step op empty"));

  if (needs_resource_arg(st.op)) {
    if (st.args.empty() || st.args.front().nested.has_value() || st.args.front().value.empty()) {
      return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": missing resource argument for " + st.op));
    }

    const auto it = resources.find(st.args.front().value);
    if (it == resources.end()) return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": undefined resource " + st.args.front().value));

    const auto want = kind_for_operation(st.op);
    if (want.has_value() && it->second != *want) {
      return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": " + st.op + " requires resource kind " + std::string(*want)));
    }
  }

  if ((st.op == "encode" || st.op == "decode") && st.args.empty()) {
    return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": " + st.op + " requires a type or nested argument"));
  }

  for (const auto& arg : st.args) {
    if (arg.nested.has_value()) {
      if (!arg.value.empty()) return Result<bool>::from_err(make_error(Error::Code::Validation, std::string(where) + ": nested arg cannot also have value"));
      auto nested_ok = validate_step_semantics(*(*arg.nested), resources, visited, where);
      if (nested_ok.is_err()) return nested_ok;
    }
  }

  visited.erase(&st);
  return Result<bool>::from_ok(true);
}

} // namespace detail

namespace backend {
using rule_key = std::variant<dsl::pattern<"recv">, dsl::pattern<"send">, dsl::pattern<"enqueue">, dsl::pattern<"dequeue">, dsl::pattern<"decode">, dsl::pattern<"encode">>;

inline auto emit_step(const ir::Step& s, const ir::BackendSpec& spec)->std::string {
  const auto by_op = detail::rule_map(spec);
  if (auto it = by_op.find(s.op); it != by_op.end()) return detail::render_emit_template(it->second, s);
  return detail::emit_default_step(s);
}

inline auto emit(const ir::Program& p, const ir::BackendSpec& spec)->Result<std::string> {
  if(spec.target.empty()) return Result<std::string>::from_err(make_error(Error::Code::Emission,"backend target empty"));

  std::string out = "# target " + spec.target + "\n";
  for (const auto& cap : spec.capabilities) out += "# capability " + cap + "\n";

  for (const auto& pipe : p.pipes) {
    out += "# pipe " + pipe.name + "\n";
    for (const auto& st : pipe.steps) out += emit_step(st, spec);
  }
  return Result<std::string>::from_ok(std::move(out));
}
} // namespace backend

inline auto validate(const ir::Program& p)->Result<ir::Program> {
  std::unordered_map<std::string, std::string> resources;
  resources.reserve(p.resources.size());

  for (const auto& r : p.resources) {
    if (r.name.empty() || r.kind.empty()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "resource empty"));
    if (resources.find(r.name) != resources.end()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "duplicate resource: " + r.name));
    resources[r.name] = r.kind;
  }

  std::unordered_set<std::string> pipe_names;
  for (const auto& pipe : p.pipes) {
    if (pipe.name.empty()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "pipe name empty"));
    if (!pipe_names.insert(pipe.name).second) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "duplicate pipe: " + pipe.name));
    if (pipe.steps.empty()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "pipe has no steps: " + pipe.name));

    for (const auto& st : pipe.steps) {
      std::unordered_set<const ir::Step*> visited;
      auto ok = detail::validate_step_semantics(st, resources, visited, pipe.name);
      if (ok.is_err()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation, "invalid step in pipe: " + pipe.name));
    }
  }

  return Result<ir::Program>::from_ok(p);
}

inline auto validate(const ir::BackendSpec& s)->Result<ir::BackendSpec> {
  if(s.target.empty()) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Validation,"target empty"));

  std::unordered_set<std::string> seen_ops;
  for (const auto& rule : s.rules) {
    if (rule.operation.empty()) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Validation, "backend rule operation empty"));
    if (rule.emit.empty()) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Validation, "backend rule emit empty for " + rule.operation));
    if (!seen_ops.insert(rule.operation).second) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Validation, "duplicate backend rule: " + rule.operation));
  }

  return Result<ir::BackendSpec>::from_ok(s);
}

inline auto validate_compatibility(const ir::Program& p, const ir::BackendSpec& s)->Result<bool> {
  std::unordered_map<std::string, std::string> resources;
  for (const auto& r : p.resources) resources[r.name] = r.kind;

  std::function<Result<bool>(const ir::Step&, const std::string&)> check_step;
  check_step = [&](const ir::Step& st, const std::string& pipe_name) -> Result<bool> {
    if (!detail::is_builtin_op(st.op) && !detail::has_rule_for(s, st.op)) {
      return Result<bool>::from_err(make_error(Error::Code::Validation, "no backend rule for operation: " + st.op + " in pipe " + pipe_name));
    }

    if (auto cap = detail::op_capability(st.op); cap.has_value()) {
      if (s.capabilities.find(std::string(*cap)) == s.capabilities.end()) {
        return Result<bool>::from_err(make_error(Error::Code::Validation, "backend missing capability: " + std::string(*cap) + " for op " + st.op));
      }
    }

    if (detail::needs_resource_arg(st.op)) {
      if (st.args.empty() || st.args.front().nested.has_value()) {
        return Result<bool>::from_err(make_error(Error::Code::Validation, "operation requires plain resource argument: " + st.op));
      }
      auto it = resources.find(st.args.front().value);
      if (it == resources.end()) {
        return Result<bool>::from_err(make_error(Error::Code::Validation, "undefined resource reference: " + st.args.front().value));
      }
      if (auto kind = detail::kind_for_operation(st.op); kind.has_value() && it->second != *kind) {
        return Result<bool>::from_err(make_error(Error::Code::Validation, "resource/operation mismatch for " + st.op + ": expected " + std::string(*kind) + ", got " + it->second));
      }
    }

    for (const auto& arg : st.args) {
      if (arg.nested.has_value()) {
        auto nested = check_step(*(*arg.nested), pipe_name);
        if (nested.is_err()) return nested;
      }
    }

    return Result<bool>::from_ok(true);
  };

  for (const auto& pipe : p.pipes) {
    if (pipe.steps.empty()) return Result<bool>::from_err(make_error(Error::Code::Validation, "empty pipe: " + pipe.name));
    for (const auto& st : pipe.steps) {
      auto ok = check_step(st, pipe.name);
      if (ok.is_err()) return ok;
    }
  }

  return Result<bool>::from_ok(true);
}

inline auto compile(const ir::Program& p, const ir::BackendSpec& s)->Result<std::string> {
  auto pv = validate(p);
  if (pv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation, "invalid program"));

  auto sv = validate(s);
  if (sv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation, "invalid backend"));

  auto cv = validate_compatibility(p, s);
  if (cv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation, "backend/program compatibility validation failed"));

  auto lowered = detail::lower_program(p, s);
  if (lowered.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Lowering, "lowering failed"));

  std::string out;
  out += "# target " + s.target + "\n";
  for (const auto& cap : s.capabilities) out += "# capability " + cap + "\n";
  out += "# scheduling polyflow=";
#if IPCTK_HAS_POLYFLOW
  out += "enabled";
#else
  out += "disabled";
#endif
  out += " workers=" + std::to_string(lowered.unwrap().worker_count) + "\n";

  const auto lowered_result = lowered.unwrap();
  for (std::size_t pi = 0; pi < p.pipes.size(); ++pi) {
    out += "# pipe " + p.pipes[pi].name + "\n";
    for (const auto& line : lowered_result.emitted[pi]) out += line;
  }

  return Result<std::string>::from_ok(std::move(out));
}

} // namespace ipctk

#endif
