#ifndef IPCTK_HPP
#define IPCTK_HPP

#include "DSLUtils.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>
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
  bool line = false, block = false;
  for (std::size_t i = 0; i < src.size(); ++i) {
    const char c = src[i];
    const char n = (i + 1 < src.size()) ? src[i + 1] : '\0';
    if (!line && !block && c == '/' && n == '/') { line = true; ++i; continue; }
    if (!line && !block && c == '/' && n == '*') { block = true; ++i; continue; }
    if (line && c == '\n') { line = false; out.push_back('\n'); continue; }
    if (block && c == '*' && n == '/') { block = false; ++i; continue; }
    if (!line && !block) out.push_back(c);
  }
  return out;
}
inline auto ws() {
  return dsl::parser([](dsl::ParsecInput& in)->dsl::ExpectedResult<char>{
    while (!in.eof() && (in.peek()==' '||in.peek()=='\t'||in.peek()=='\n'||in.peek()=='\r')) in.consume();
    return '\0';
  });
}
inline auto ident() {
  return dsl::parser([](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    while (!in.eof() && (in.peek()==' '||in.peek()=='\t'||in.peek()=='\n'||in.peek()=='\r')) in.consume();
    if (in.eof() || !(std::isalpha((unsigned char)in.peek())||in.peek()=='_')) return dsl::fail_expected<std::string>(in,"identifier");
    std::string out; out.push_back(in.consume());
    while (!in.eof() && (std::isalnum((unsigned char)in.peek())||in.peek()=='_'||in.peek()=='.'||in.peek()=='-')) out.push_back(in.consume());
    return out;
  });
}
inline auto token(char c){ return dsl::parser([c](dsl::ParsecInput& in)->dsl::ExpectedResult<char>{ while(!in.eof()&&std::isspace((unsigned char)in.peek())) in.consume(); if(!in.eof()&&in.peek()==c) return in.consume(); return dsl::fail_expected<char>(in,std::string(1,c));}); }
inline auto until(char endc) {
  return dsl::parser([endc](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    std::string out;
    int depth = 0;
    while (!in.eof()) {
      char c = in.peek();
      if (c == '(') depth++;
      if (c == ')') depth--;
      if (depth <= 0 && c == endc) break;
      out.push_back(in.consume());
    }
    return out;
  });
}
inline auto read_rule_body(dsl::ParsecInput& in) -> dsl::ExpectedResult<std::string> {
  while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) in.consume();
  if (in.eof()) return dsl::fail_expected<std::string>(in, "rule-body");
  if (in.peek() == '"' && in.pos + 2 < in.source.size() && in.source[in.pos + 1] == '"' && in.source[in.pos + 2] == '"') {
    in.consume(); in.consume(); in.consume();
    std::string out;
    while (!in.eof()) {
      if (in.peek() == '"' && in.pos + 2 < in.source.size() && in.source[in.pos + 1] == '"' && in.source[in.pos + 2] == '"') {
        in.consume(); in.consume(); in.consume();
        return out;
      }
      out.push_back(in.consume());
    }
    return dsl::fail_expected<std::string>(in, "closing-triple-quote");
  }
  if (in.peek() == '{') {
    in.consume();
    int depth = 1;
    std::string out;
    while (!in.eof()) {
      char c = in.consume();
      if (c == '{') ++depth;
      if (c == '}') {
        --depth;
        if (depth == 0) return out;
      }
      out.push_back(c);
    }
    return dsl::fail_expected<std::string>(in, "closing-brace");
  }
  return until('\n')(in);
}
inline auto split_top_level(std::string_view src, std::string_view delim) -> std::vector<std::string> {
  std::vector<std::string> out;
  int depth = 0;
  std::size_t start = 0;
  for (std::size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '(') ++depth;
    else if (src[i] == ')' && depth > 0) --depth;
    if (depth == 0 && i + delim.size() <= src.size() && src.substr(i, delim.size()) == delim) {
      out.emplace_back(trim(src.substr(start, i - start)));
      i += delim.size() - 1;
      start = i + 1;
    }
  }
  out.emplace_back(trim(src.substr(start)));
  return out;
}
} // namespace parse

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
  auto open = src.find('('); auto close = src.rfind(')');
  if (open == std::string_view::npos || close == std::string_view::npos || close <= open) return std::nullopt;
  ir::Step st{}; st.op = std::string(parse::trim(src.substr(0, open)));
  auto inside = src.substr(open + 1, close - open - 1);
  for (auto& token : parse::split_top_level(inside, ",")) {
    if (token.empty()) continue;
    if (auto nested = parse_step(token)) st.args.push_back({"", std::make_shared<ir::Step>(std::move(*nested))});
    else st.args.push_back({std::string(parse::trim(token)), std::nullopt});
  }
  return st;
}

inline auto parse_text(std::string_view source) -> Result<ir::Program> {
  using namespace parse;
  const auto clean = strip_comments(source);
  auto p = dsl::parser([&](dsl::ParsecInput& in)->dsl::ExpectedResult<ir::Program>{
    ir::Program out{};
    while (!in.eof()) {
      ws()(in);
      if (in.eof()) break;
      auto kw = ident()(in); if(!kw) return dsl::ExpectedResult<ir::Program>::failure(kw.error.pos, kw.error.kind, kw.error.expected);
      if (*kw == "pipe") {
        auto nm = ident()(in); if(!nm) return dsl::ExpectedResult<ir::Program>::failure(nm.error.pos,nm.error.kind,nm.error.expected);
        if(!token('=')(in)) return dsl::fail_expected<ir::Program>(in,"=");
        auto rhs = until(';')(in); if(!rhs) return dsl::ExpectedResult<ir::Program>::failure(rhs.error.pos, rhs.error.kind, rhs.error.expected); if(!token(';')(in)) return dsl::fail_expected<ir::Program>(in,";");
        ir::Pipe pp{*nm,{}};
        for (const auto& part : split_top_level(*rhs, "->")) {
          if (part.empty()) continue;
          auto st = parse_step(part);
          if (!st) return dsl::fail_expected<ir::Program>(in, "pipe-step");
          pp.steps.push_back(std::move(*st));
        }
        if (pp.steps.empty()) return dsl::fail_expected<ir::Program>(in, "non-empty-pipe");
        out.pipes.push_back(std::move(pp));
      } else {
        auto nm = ident()(in); if(!nm) return dsl::ExpectedResult<ir::Program>::failure(nm.error.pos,nm.error.kind,nm.error.expected);
        if(!token('=')(in)) return dsl::fail_expected<ir::Program>(in,"=");
        auto init = until(';')(in); if(!init) return dsl::ExpectedResult<ir::Program>::failure(init.error.pos, init.error.kind, init.error.expected); if(!token(';')(in)) return dsl::fail_expected<ir::Program>(in,";");
        out.resources.push_back(ir::Resource{*kw, *nm, std::string(trim(*init))});
      }
    }
    out.ast = dsl::node<"ipcl.program">(dsl::leaf<"resource_count">((int)out.resources.size()), dsl::leaf<"pipe_count">((int)out.pipes.size()));
    return out;
  });
  auto res = dsl::run_parser(p, clean);
  if (!res.value) return Result<ir::Program>::from_err(make_error(Error::Code::Parse, "ipcl parse failed, expected: " + (res.error.expected.empty()?std::string("token"):res.error.expected.front()), res.error.pos));
  return Result<ir::Program>::from_ok(*res.value);
}
} // namespace ipcl

namespace itkd {
struct NativeDSL : dsl::DSL<NativeDSL, dsl::Pipeline, dsl::Rewrite, dsl::PatternMatch, dsl::Operators, dsl::CombinatorParser, dsl::AST> {};
struct BackendBuilder { ir::BackendSpec spec{}; auto target(std::string v)->BackendBuilder&{spec.target=std::move(v);return *this;} auto capability(std::string v)->BackendBuilder&{spec.capabilities.insert(std::move(v));return *this;} auto rule(std::string op, std::string emit)->BackendBuilder&{spec.rules.push_back({std::move(op),std::move(emit),dsl::node<"rule">()}); return *this;} auto build()->ir::BackendSpec{ spec.ast = dsl::node<"itkd.backend">(dsl::leaf<"target">(spec.target)); return spec; } };
inline auto backend()->BackendBuilder { return {}; }
inline auto parse_text(std::string_view source)->Result<ir::BackendSpec> {
  using namespace parse;
  const auto clean = strip_comments(source);
  auto p = dsl::parser([&](dsl::ParsecInput& in)->dsl::ExpectedResult<ir::BackendSpec>{
    ir::BackendSpec spec{};
    while(!in.eof()) {
      ws()(in); if(in.eof()) break;
      auto kw = ident()(in); if(!kw) return dsl::ExpectedResult<ir::BackendSpec>::failure(kw.error.pos,kw.error.kind,kw.error.expected);
      if(*kw=="target") { auto id=ident()(in); if(!id) return dsl::ExpectedResult<ir::BackendSpec>::failure(id.error.pos,id.error.kind,id.error.expected); spec.target=*id; }
      else if(*kw=="capability") { auto id=ident()(in); if(!id) return dsl::ExpectedResult<ir::BackendSpec>::failure(id.error.pos,id.error.kind,id.error.expected); spec.capabilities.insert(*id); }
      else if(*kw=="import" || *kw=="include" || *kw=="map") {
        auto body = until('\n')(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
      } else if(*kw=="rule") {
        auto op=ident()(in); if(!op) return dsl::ExpectedResult<ir::BackendSpec>::failure(op.error.pos,op.error.kind,op.error.expected);
        if(!token('=')(in)) return dsl::fail_expected<ir::BackendSpec>(in,"=");
        auto body = read_rule_body(in);
        if (!body) return dsl::ExpectedResult<ir::BackendSpec>::failure(body.error.pos, body.error.kind, body.error.expected);
        spec.rules.push_back({*op,std::string(trim(*body)),dsl::node<"rule">(dsl::leaf<"op">(*op))});
      } else {
        return dsl::fail_expected<ir::BackendSpec>(in, "target/capability/rule/import/include/map");
      }
      while(!in.eof() && in.peek()!='\n') in.consume(); if(!in.eof()) in.consume();
    }
    spec.ast = dsl::node<"itkd.spec">(dsl::leaf<"target">(spec.target));
    return spec;
  });
  auto o = dsl::run_parser(p, clean);
  if(!o.value) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Parse,"itkd parse failed, expected: " + (o.error.expected.empty()?std::string("token"):o.error.expected.front()),o.error.pos));
  return Result<ir::BackendSpec>::from_ok(*o.value);
}
} // namespace itkd

namespace runtime {
class FdHandle {
 public:
  FdHandle() = default; explicit FdHandle(int fd):fd_(fd){} ~FdHandle(){ if(fd_>=0) ::close(fd_); }
  FdHandle(FdHandle&& o) noexcept : fd_(std::exchange(o.fd_,-1)) {}
  auto operator=(FdHandle&& o) noexcept -> FdHandle& { if(this!=&o){ if(fd_>=0) ::close(fd_); fd_=std::exchange(o.fd_,-1);} return *this; }
  FdHandle(const FdHandle&) = delete; auto operator=(const FdHandle&) -> FdHandle& = delete;
  auto get() const -> int { return fd_; }
 private: int fd_{-1};
};
struct PipePair { FdHandle read_end{}, write_end{}; };
inline auto open_pipe() -> Result<PipePair> { int fds[2]; if(::pipe(fds)!=0) return Result<PipePair>::from_err(make_error(Error::Code::Runtime,"pipe failed")); return Result<PipePair>::from_ok(PipePair{FdHandle{fds[0]}, FdHandle{fds[1]}}); }

class LocalSocket {
 public:
  static auto connect(std::string path) -> Result<LocalSocket> {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0); if (fd < 0) return Result<LocalSocket>::from_err(make_error(Error::Code::Runtime, "socket failed"));
    sockaddr_un addr{}; addr.sun_family = AF_UNIX; std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { ::close(fd); return Result<LocalSocket>::from_err(make_error(Error::Code::Runtime, "connect failed")); }
    return Result<LocalSocket>::from_ok(LocalSocket(FdHandle{fd}));
  }
  auto send(std::span<const std::byte> b)->Result<std::size_t>{ auto n=::write(fd_.get(), b.data(), b.size()); if(n<0) return Result<std::size_t>::from_err(make_error(Error::Code::Runtime,"write failed")); return Result<std::size_t>::from_ok((std::size_t)n); }
 private:
  explicit LocalSocket(FdHandle fd):fd_(std::move(fd)){} FdHandle fd_{};
};
} // namespace runtime

namespace backend {
using rule_key = std::variant<dsl::pattern<"recv">, dsl::pattern<"send">, dsl::pattern<"enqueue">, dsl::pattern<"dequeue">, dsl::pattern<"decode">, dsl::pattern<"encode">>;
inline auto emit_step(const ir::Step& s, const ir::BackendSpec& spec)->std::string {
  std::unordered_map<std::string, std::string> byop; for (const auto& r: spec.rules) byop[r.operation]=r.emit;
  if (auto it = byop.find(s.op); it != byop.end()) return it->second + "\n";
  if (dsl::pattern<"recv">::matches(s.op)) return "recv();\n";
  if (dsl::pattern<"send">::matches(s.op)) return "send();\n";
  if (dsl::pattern<"enqueue">::matches(s.op)) return "enqueue();\n";
  return "/* op */\n";
}
inline auto emit(const ir::Program& p, const ir::BackendSpec& spec)->Result<std::string> {
  if(spec.target.empty()) return Result<std::string>::from_err(make_error(Error::Code::Emission,"backend target empty"));
  std::string out = "# target " + spec.target + "\n";
  for (const auto& pipe : p.pipes) { out += "# pipe " + pipe.name + "\n"; for (const auto& st : pipe.steps) out += emit_step(st, spec); }
  return Result<std::string>::from_ok(std::move(out));
}
} // namespace backend

inline auto validate(const ir::Program& p)->Result<ir::Program> {
  std::unordered_map<std::string,std::string> kinds; for (const auto& r: p.resources) { if(r.name.empty()||r.kind.empty()) return Result<ir::Program>::from_err(make_error(Error::Code::Validation,"resource empty")); kinds[r.name]=r.kind; }
  for (const auto& pipe: p.pipes) for (const auto& st: pipe.steps) {
    auto needs = [&](std::string_view k)->bool{ return !st.args.empty() && !st.args[0].nested.has_value() && kinds[st.args[0].value]==k; };
    if ((st.op=="recv"||st.op=="send") && !needs("socket")) return Result<ir::Program>::from_err(make_error(Error::Code::Validation,"socket op mismatch"));
    if ((st.op=="enqueue"||st.op=="dequeue") && !needs("queue")) return Result<ir::Program>::from_err(make_error(Error::Code::Validation,"queue op mismatch"));
  }
  return Result<ir::Program>::from_ok(p);
}
inline auto validate(const ir::BackendSpec& s)->Result<ir::BackendSpec> { if(s.target.empty()) return Result<ir::BackendSpec>::from_err(make_error(Error::Code::Validation,"target empty")); return Result<ir::BackendSpec>::from_ok(s); }
inline auto validate_compatibility(const ir::Program& p, const ir::BackendSpec& s)->Result<bool> {
  for (const auto& pipe : p.pipes) {
    bool has_pubsub = s.capabilities.find("pubsub") != s.capabilities.end();
    for (const auto& st : pipe.steps) if ((st.op=="enqueue"||st.op=="dequeue") && !has_pubsub) return Result<bool>::from_err(make_error(Error::Code::Validation,"backend missing pubsub capability"));
  }
  return Result<bool>::from_ok(true);
}
inline auto compile(const ir::Program& p, const ir::BackendSpec& s)->Result<std::string> {
  auto pv = validate(p); if(pv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation,"invalid program"));
  auto sv = validate(s); if(sv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation,"invalid backend"));
  auto cv = validate_compatibility(p,s); if(cv.is_err()) return Result<std::string>::from_err(make_error(Error::Code::Validation,"compat failed"));
  return backend::emit(p,s);
}

} // namespace ipctk

#endif
