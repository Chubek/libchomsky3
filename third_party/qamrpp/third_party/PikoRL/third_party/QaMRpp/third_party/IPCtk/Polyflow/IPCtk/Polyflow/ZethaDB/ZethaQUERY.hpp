#pragma once
/*
  ZethaQUERY.hpp
  -------------
  XPath-like query language for ZethaDB.

  Two front-ends:
    1) Text parser: XPath-ish syntax (/ // . .. @attr [pred] fn(...) and/or/not, comparisons)
    2) Native C++ DSL: build same AST programmatically

  This header is designed to be implemented using DSLUtils.hpp:
    - derive grammar from DSLUtils::DSL (CRTP)
    - build parse AST using DSLUtils::ASTNode
    - return DSLUtils::Result<Query> (ParseFailureKind errors)

  NOTE:
    This adapter is implemented against the local DSLUtils.hpp surface and lowers a
    constrained XPath-like subset into zetha::query::Query.
*/

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <optional>
#include <memory>
#include <utility>
#include <stdexcept>
#include <initializer_list>
#include <cctype>
#include <cstdlib>
#include "../PolyflowDSLUtils.hpp"

namespace zetha::query {

// ===========================
// 1) Engine-facing Query AST
// ===========================

enum class Axis : uint8_t {
  Child,
  DescendantOrSelf, // //
  Self,             // .
  Parent,           // ..
  Attribute         // @
};

enum class NodeTestKind : uint8_t {
  Any,        // *
  Name,       // foo   or ns:foo
  WildPrefix, // ns:*
  KindTest    // text(), node()
};

struct NodeTest {
  NodeTestKind kind{NodeTestKind::Any};
  std::string  name;     // for Name/KindTest (local name or kind name)
  std::string  nsPrefix; // optional

  static NodeTest Any() { return {}; }
  static NodeTest Name(std::string local, std::string ns = {}) {
    NodeTest t; t.kind = NodeTestKind::Name; t.name = std::move(local); t.nsPrefix = std::move(ns); return t;
  }
  static NodeTest WildPrefix(std::string ns) {
    NodeTest t; t.kind = NodeTestKind::WildPrefix; t.nsPrefix = std::move(ns); return t;
  }
  static NodeTest Kind(std::string kindName) {
    NodeTest t; t.kind = NodeTestKind::KindTest; t.name = std::move(kindName); return t;
  }
};

enum class CmpOp : uint8_t { Eq, Ne, Lt, Le, Gt, Ge };
enum class BoolOp : uint8_t { And, Or };

struct Expr;
struct PathExpr;

struct Literal {
  using Value = std::variant<std::monostate, bool, double, std::string>;
  Value value;

  static Literal Null() { return Literal{std::monostate{}}; }
  static Literal Bool(bool b) { return Literal{b}; }
  static Literal Number(double d) { return Literal{d}; }
  static Literal String(std::string s) { return Literal{std::move(s)}; }
};

struct AttributeRef { std::string name; };
struct NameRef      { std::string name; };

struct FunctionCall {
  std::string name;
  std::vector<Expr> args;
};

struct Compare {
  CmpOp op{};
  std::shared_ptr<Expr> lhs;
  std::shared_ptr<Expr> rhs;
};

struct BoolBin {
  BoolOp op{};
  std::shared_ptr<Expr> lhs;
  std::shared_ptr<Expr> rhs;
};

struct NotExpr { std::shared_ptr<Expr> inner; };

struct Expr {
  using V = std::variant<
    Literal,
    AttributeRef,
    NameRef,
    FunctionCall,
    Compare,
    BoolBin,
    NotExpr,
    std::shared_ptr<PathExpr>
  >;

  V v;
  Expr() = default;
  template <class T> Expr(T x) : v(std::move(x)) {}
};

struct Predicate { Expr expr; };

struct Step {
  Axis axis{Axis::Child};
  NodeTest test{NodeTest::Any()};
  std::vector<Predicate> predicates;
};

struct PathExpr {
  bool absolute{false}; // begins with '/'
  std::vector<Step> steps;
};

struct OrderByKey { Expr key; bool descending{false}; };

struct Query {
  PathExpr path;
  std::vector<OrderByKey> orderBy;
  std::optional<std::size_t> limit;
  std::optional<std::size_t> offset;
};

// ===========================
// 2) Native C++ DSL builder
// ===========================

namespace dsl {

struct ExprB {
  Expr e;
  ExprB() = default;
  explicit ExprB(Expr x) : e(std::move(x)) {}

  friend ExprB operator!(ExprB a) {
    return ExprB{Expr{NotExpr{std::make_shared<Expr>(std::move(a.e))}}};
  }
  friend ExprB operator&&(ExprB a, ExprB b) {
    return ExprB{Expr{BoolBin{BoolOp::And,
      std::make_shared<Expr>(std::move(a.e)),
      std::make_shared<Expr>(std::move(b.e))}}};
  }
  friend ExprB operator||(ExprB a, ExprB b) {
    return ExprB{Expr{BoolBin{BoolOp::Or,
      std::make_shared<Expr>(std::move(a.e)),
      std::make_shared<Expr>(std::move(b.e))}}};
  }

  static ExprB cmp(CmpOp op, ExprB a, ExprB b) {
    return ExprB{Expr{Compare{op,
      std::make_shared<Expr>(std::move(a.e)),
      std::make_shared<Expr>(std::move(b.e))}}};
  }

  friend ExprB operator==(ExprB a, ExprB b) { return cmp(CmpOp::Eq, std::move(a), std::move(b)); }
  friend ExprB operator!=(ExprB a, ExprB b) { return cmp(CmpOp::Ne, std::move(a), std::move(b)); }
  friend ExprB operator< (ExprB a, ExprB b) { return cmp(CmpOp::Lt, std::move(a), std::move(b)); }
  friend ExprB operator<=(ExprB a, ExprB b) { return cmp(CmpOp::Le, std::move(a), std::move(b)); }
  friend ExprB operator> (ExprB a, ExprB b) { return cmp(CmpOp::Gt, std::move(a), std::move(b)); }
  friend ExprB operator>=(ExprB a, ExprB b) { return cmp(CmpOp::Ge, std::move(a), std::move(b)); }
};

inline ExprB lit(std::string s) { return ExprB{Expr{Literal::String(std::move(s))}}; }
inline ExprB lit(const char* s) { return lit(std::string(s)); }
inline ExprB lit(double d)      { return ExprB{Expr{Literal::Number(d)}}; }
inline ExprB lit(std::int64_t i){ return lit(static_cast<double>(i)); }
inline ExprB lit(bool b)        { return ExprB{Expr{Literal::Bool(b)}}; }

inline ExprB attr(std::string n) { return ExprB{Expr{AttributeRef{std::move(n)}}}; }
inline ExprB attr(const char* n) { return attr(std::string(n)); }

inline ExprB name(std::string n) { return ExprB{Expr{NameRef{std::move(n)}}}; }
inline ExprB name(const char* n) { return name(std::string(n)); }

inline ExprB fn(std::string n, std::initializer_list<ExprB> args = {}) {
  FunctionCall f; f.name = std::move(n);
  f.args.reserve(args.size());
  for (auto& a : args) f.args.push_back(a.e);
  return ExprB{Expr{std::move(f)}};
}

struct PathB {
  Query q;

  PathB& order_by(ExprB key, bool desc=false) {
    q.orderBy.push_back(OrderByKey{std::move(key.e), desc});
    return *this;
  }
  PathB& limit(std::size_t n)  { q.limit = n; return *this; }
  PathB& offset(std::size_t n) { q.offset = n; return *this; }

  Query build() { return std::move(q); }
};

struct StepB {
  PathB p;

  explicit StepB(bool abs) { p.q.path.absolute = abs; }

  StepB& step(Axis ax, NodeTest test) {
    Step s; s.axis = ax; s.test = std::move(test);
    p.q.path.steps.push_back(std::move(s));
    return *this;
  }
  StepB& where(ExprB e) {
    if (p.q.path.steps.empty()) throw std::logic_error("where(): no step");
    p.q.path.steps.back().predicates.push_back(Predicate{std::move(e.e)});
    return *this;
  }

  StepB& child(std::string n) { return step(Axis::Child, NodeTest::Name(std::move(n))); }
  StepB& desc (std::string n) { return step(Axis::DescendantOrSelf, NodeTest::Name(std::move(n))); }
  StepB& any_child()          { return step(Axis::Child, NodeTest::Any()); }
  StepB& self()               { return step(Axis::Self, NodeTest::Any()); }
  StepB& parent()             { return step(Axis::Parent, NodeTest::Any()); }
  StepB& attribute(std::string n) { return step(Axis::Attribute, NodeTest::Name(std::move(n))); }

  PathB& query() { return p; }
  const PathB& query() const { return p; }
};

inline StepB root() { return StepB(true); }
inline StepB rel()  { return StepB(false); }

inline ExprB path_expr(const StepB& b) {
  return ExprB{Expr{std::make_shared<PathExpr>(b.query().q.path)}};
}

} // namespace dsl

// =====================================
// 3) Text parser (DSLUtils integration)
// =====================================

namespace text {

// This section must bind to DSLUtils.hpp.
// I cannot finalize it without the exact declarations.
// So I define an adapter interface you will implement.

using ParseResult = ::dsl::Result<Query, ::dsl::ParseError>;

class Parser {
public:
  ParseResult parse(std::string_view input) const {
    ::dsl::ParsecInput in{input, 0};
    auto parsed = parse_path(in);
    if (!parsed) return ParseResult::from_err(parsed.error);
    skip_ws(in);
    if (!in.eof()) {
      return ParseResult::from_err(::dsl::ParseError{in.pos, ::dsl::ParseFailureKind::Committed, {"end-of-input"}});
    }
    return lower_query(*parsed);
  }

  static ParseResult lower_from_ast(const ::dsl::ASTNode& ast) {
    return lower_query(ast);
  }
private:
  static void skip_ws(::dsl::ParsecInput& in) {
    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) in.consume();
  }
  static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
  }
  static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> fail_at(std::size_t pos, std::string expected) {
    return ::dsl::ExpectedResult<::dsl::ASTNode>::failure(pos, ::dsl::ParseFailureKind::Committed, {std::move(expected)});
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> parse_identifier(::dsl::ParsecInput& in) {
    skip_ws(in);
    const std::size_t start = in.pos;
    if (in.eof() || !is_ident_start(in.peek())) return ::dsl::ExpectedResult<::dsl::ASTNode>::failure(start, ::dsl::ParseFailureKind::Committed, {"identifier"});
    std::string ident;
    ident.push_back(in.consume());
    while (!in.eof() && is_ident_char(in.peek())) ident.push_back(in.consume());
    return ::dsl::leaf<"identifier">(ident);
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> parse_literal(::dsl::ParsecInput& in) {
    skip_ws(in);
    const std::size_t start = in.pos;
    if (!in.eof() && (in.peek() == '\'' || in.peek() == '"')) {
      const char q = in.consume();
      std::string s;
      while (!in.eof() && in.peek() != q) s.push_back(in.consume());
      if (in.eof()) return fail_at(start, "closing-quote");
      in.consume();
      return ::dsl::leaf<"lit_string">(s);
    }
    if (!in.eof() && (std::isdigit(static_cast<unsigned char>(in.peek())) || in.peek() == '-')) {
      std::string num;
      if (in.peek() == '-') num.push_back(in.consume());
      if (in.eof() || !std::isdigit(static_cast<unsigned char>(in.peek()))) return fail_at(in.pos, "number");
      while (!in.eof() && std::isdigit(static_cast<unsigned char>(in.peek()))) num.push_back(in.consume());
      return ::dsl::leaf<"lit_number">(num);
    }
    auto id = parse_identifier(in);
    if (!id) return id;
    const auto& name = id.value.value().value();
    if (name == "true" || name == "false") return ::dsl::leaf<"lit_bool">(name);
    return id;
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> parse_predicate(::dsl::ParsecInput& in) {
    skip_ws(in);
    const std::size_t start = in.pos;
    if (in.eof() || in.peek() != '[') return fail_at(start, "[");
    in.consume();
    skip_ws(in);
    if (in.eof() || in.peek() != '@') return fail_at(in.pos, "@");
    in.consume();
    auto attr = parse_identifier(in);
    if (!attr) return attr;
    skip_ws(in);
    if (in.eof() || in.peek() != '=') return fail_at(in.pos, "=");
    in.consume();
    auto lit = parse_literal(in);
    if (!lit) return lit;
    skip_ws(in);
    if (in.eof() || in.peek() != ']') return fail_at(in.pos, "]");
    in.consume();
    std::vector<::dsl::ASTNode> cmp_children;
    cmp_children.push_back(attr.value.value());
    cmp_children.push_back(lit.value.value());
    std::vector<::dsl::ASTNode> pred_children;
    pred_children.emplace_back("attr_eq", std::move(cmp_children));
    return ::dsl::ASTNode{"predicate", std::move(pred_children)};
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> parse_segment(::dsl::ParsecInput& in) {
    auto id = parse_identifier(in);
    if (!id) return id;
    std::vector<::dsl::ASTNode> children;
    children.push_back(id.value.value());
    while (true) {
      skip_ws(in);
      if (in.eof() || in.peek() != '[') break;
      auto pred = parse_predicate(in);
      if (!pred) return pred;
      children.push_back(pred.value.value());
    }
    return ::dsl::ASTNode{"segment", std::move(children)};
  }
  static ::dsl::ExpectedResult<::dsl::ASTNode> parse_path(::dsl::ParsecInput& in) {
    skip_ws(in);
    const std::size_t start = in.pos;
    std::vector<::dsl::ASTNode> children;
    bool absolute = false;
    if (!in.eof() && in.peek() == '/') {
      absolute = true;
      in.consume();
    }
    auto first = parse_segment(in);
    if (!first) return first;
    children.push_back(first.value.value());
    while (true) {
      skip_ws(in);
      if (in.eof()) break;
      if (in.peek() != '/') break;
      in.consume();
      skip_ws(in);
      if (in.eof() || in.peek() == '/') {
        return ::dsl::ExpectedResult<::dsl::ASTNode>::failure(in.pos, ::dsl::ParseFailureKind::Committed, {"identifier"});
      }
      auto segment = parse_segment(in);
      if (!segment) return segment;
      children.push_back(segment.value.value());
    }
    if (absolute) children.insert(children.begin(), ::dsl::leaf<"absolute">("true"));
    return ::dsl::ASTNode{"path", std::move(children)};
  }
  static ParseResult lower_query(const ::dsl::ASTNode& ast) {
    if (ast.tag() != "path") {
      return ParseResult::from_err(::dsl::ParseError{0, ::dsl::ParseFailureKind::Committed, {"path"}});
    }
    Query q{};
    std::size_t idx = 0;
    const auto& kids = ast.children();
    if (!kids.empty() && kids.front().tag() == "absolute") {
      q.path.absolute = true;
      idx = 1;
    }
    for (; idx < kids.size(); ++idx) {
      if (kids[idx].tag() != "segment") {
        return ParseResult::from_err(::dsl::ParseError{idx, ::dsl::ParseFailureKind::Committed, {"segment"}});
      }
      const auto& seg_children = kids[idx].children();
      if (seg_children.empty() || seg_children.front().tag() != "identifier") {
        return ParseResult::from_err(::dsl::ParseError{idx, ::dsl::ParseFailureKind::Committed, {"identifier"}});
      }
      Step step{Axis::Child, NodeTest::Name(seg_children.front().value()), {}};
      for (std::size_t pi = 1; pi < seg_children.size(); ++pi) {
        const auto& pred = seg_children[pi];
        if (pred.tag() != "predicate" || pred.children().size() != 1) {
          return ParseResult::from_err(::dsl::ParseError{pi, ::dsl::ParseFailureKind::Committed, {"predicate"}});
        }
        const auto& cmp = pred.children().front();
        if (cmp.tag() != "attr_eq" || cmp.children().size() != 2 || cmp.children()[0].tag() != "identifier") {
          return ParseResult::from_err(::dsl::ParseError{pi, ::dsl::ParseFailureKind::Committed, {"attr_eq"}});
        }
        Expr lhs = AttributeRef{cmp.children()[0].value()};
        Expr rhs;
        const auto& lit = cmp.children()[1];
        if (lit.tag() == "lit_string") rhs = Literal::String(lit.value());
        else if (lit.tag() == "lit_number") rhs = Literal::Number(std::strtod(lit.value().c_str(), nullptr));
        else if (lit.tag() == "lit_bool") rhs = Literal::Bool(lit.value() == "true");
        else if (lit.tag() == "identifier") rhs = NameRef{lit.value()};
        else return ParseResult::from_err(::dsl::ParseError{pi, ::dsl::ParseFailureKind::Committed, {"literal"}});
        step.predicates.push_back(Predicate{Expr{Compare{
          CmpOp::Eq, std::make_shared<Expr>(std::move(lhs)), std::make_shared<Expr>(std::move(rhs))
        }}});
      }
      q.path.steps.push_back(std::move(step));
    }
    return ParseResult::from_ok(std::move(q));
  }
};

} // namespace text

} // namespace zetha::query
