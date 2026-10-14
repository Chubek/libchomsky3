#include "../../ZethaQUERY.hpp"
#include <cstdlib>
#include <iostream>

static int failures = 0;
static void expect(bool cond, const char* msg) { if (!cond) { std::cerr << msg << "\n"; ++failures; } }

int main() {
  zetha::query::text::Parser p;

  auto a = p.parse("users");
  expect(a.is_ok(), "valid single segment should parse");

  auto b = p.parse("users/orders");
  expect(b.is_ok(), "valid multi segment should parse");

  auto c = p.parse("");
  expect(c.is_err(), "empty input should fail");

  auto d = p.parse("users/$bad");
  expect(d.is_err(), "invalid character should fail");

  auto e = p.parse("users/");
  expect(e.is_err(), "trailing slash should fail");

  auto f = p.parse("/users[@id=42]/orders[@status='open']");
  expect(f.is_ok(), "predicate path should parse");
  if (f.is_ok()) {
    auto q = f.unwrap();
    expect(q.path.absolute, "absolute path should set absolute=true");
    expect(q.path.steps.size() == 2, "predicate path should have two steps");
    expect(q.path.steps[0].predicates.size() == 1, "first step should have one predicate");
    expect(q.path.steps[1].predicates.size() == 1, "second step should have one predicate");
  }

  auto g = p.parse("users[@id=]");
  expect(g.is_err(), "predicate missing literal should fail");

  auto h = p.parse("users[@id=1");
  expect(h.is_err(), "unterminated predicate should fail");

  dsl::ASTNode bad_root{"segment", std::vector<dsl::ASTNode>{dsl::ASTNode{"identifier", "users"}}};
  auto i = zetha::query::text::Parser::lower_from_ast(bad_root);
  expect(i.is_err(), "lowering should reject non-path root");

  dsl::ASTNode bad_segment{"path", std::vector<dsl::ASTNode>{dsl::ASTNode{"segment", std::vector<dsl::ASTNode>{}}}};
  auto j = zetha::query::text::Parser::lower_from_ast(bad_segment);
  expect(j.is_err(), "lowering should reject segment without identifier");

  dsl::ASTNode bad_predicate{
    "path",
    std::vector<dsl::ASTNode>{
      dsl::ASTNode{
        "segment",
        std::vector<dsl::ASTNode>{
          dsl::ASTNode{"identifier", "users"},
          dsl::ASTNode{"predicate", std::vector<dsl::ASTNode>{dsl::ASTNode{"attr_eq", std::vector<dsl::ASTNode>{dsl::ASTNode{"identifier", "id"}}}}}
        }
      }
    }
  };
  auto k = zetha::query::text::Parser::lower_from_ast(bad_predicate);
  expect(k.is_err(), "lowering should reject malformed predicate AST");

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
