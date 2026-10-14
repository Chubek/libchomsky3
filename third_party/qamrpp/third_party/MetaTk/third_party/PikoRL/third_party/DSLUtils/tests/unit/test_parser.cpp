#include "../../DSLUtils.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ParsecInput primitives") {
  dsl::ParsecInput in{"abc", 0};
  REQUIRE(in.peek() == 'a');
  REQUIRE_FALSE(in.eof());
  REQUIRE(in.consume() == 'a');
  REQUIRE(in.pos == 1);
  REQUIRE(in.get_span(0) == "a");
}

TEST_CASE("Parser success value") {
  auto p = dsl::parser([](dsl::ParsecInput& i) -> dsl::ExpectedResult<char> {
    if (i.peek() == 'x') return i.consume();
    return std::nullopt;
  });
  dsl::ParsecInput in{"xyz", 0};
  auto r = p(in);
  REQUIRE(r);
  REQUIRE(*r == 'x');
}

TEST_CASE("Soft failure kind by default") {
  auto p = dsl::parser([](dsl::ParsecInput&) -> dsl::ExpectedResult<char> {
    return std::nullopt;
  });
  dsl::ParsecInput in{"", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Soft);
}

TEST_CASE("Sequence commits on rhs failure after consumption") {
  auto a = dsl::ch('a');
  auto b = dsl::ch('b');
  auto p = a & b;
  dsl::ParsecInput in{"ac", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Committed);
  REQUIRE(in.pos == 0);
}

TEST_CASE("Alternative tries rhs on soft lhs failure") {
  auto p = dsl::ch('a') | dsl::ch('b');
  dsl::ParsecInput in{"b", 0};
  auto r = p(in);
  REQUIRE(r);
  REQUIRE(*r == 'b');
}

TEST_CASE("Alternative does not try rhs on committed lhs failure") {
  auto lhs = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    if (in.peek() == 'a') {
      in.consume();
      return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Committed, {"forced"});
    }
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Soft, {"a"});
  });
  auto rhs = dsl::ch('z');
  auto p = lhs | rhs;
  dsl::ParsecInput in{"a", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Committed);
  REQUIRE(r.error.expected[0] == "forced");
}

TEST_CASE("optional returns empty on soft fail") {
  auto p = dsl::optional(dsl::ch('x'));
  dsl::ParsecInput in{"abc", 0};
  auto r = p(in);
  REQUIRE(r);
  REQUIRE_FALSE((*r).has_value());
  REQUIRE(in.pos == 0);
}

TEST_CASE("optional propagates committed fail") {
  auto p0 = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    in.consume();
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Committed, {"x"});
  });
  auto p = dsl::optional(p0);
  dsl::ParsecInput in{"a", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Committed);
}

TEST_CASE("many collects values") {
  auto p = *dsl::ch('a');
  dsl::ParsecInput in{"aaab", 0};
  auto r = p(in);
  REQUIRE(r);
  REQUIRE((*r).size() == 3);
  REQUIRE(in.pos == 3);
}

TEST_CASE("many propagates committed failures") {
  auto p0 = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    if (in.peek() == 'a') return in.consume();
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Committed, {"a"});
  });
  auto p = *p0;
  dsl::ParsecInput in{"aab", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Committed);
}

TEST_CASE("try_parse rolls back and softens") {
  auto p0 = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    in.consume();
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Committed, {"x"});
  });
  auto p = dsl::try_parse(p0);
  dsl::ParsecInput in{"a", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(in.pos == 0);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Soft);
}

TEST_CASE("labeled overwrites expected list") {
  auto p = dsl::labeled(dsl::ch('q'), "letter-q");
  dsl::ParsecInput in{"x", 0};
  auto r = p(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.expected.size() == 1);
  REQUIRE(r.error.expected[0] == "letter-q");
}

TEST_CASE("run_parser success requires eof") {
  auto p = dsl::ch('a');
  auto out = dsl::run_parser(p, "a");
  REQUIRE(out.value.has_value());
}

TEST_CASE("run_parser reports trailing input") {
  auto p = dsl::ch('a');
  auto out = dsl::run_parser(p, "ab");
  REQUIRE_FALSE(out.value.has_value());
  REQUIRE(out.error.kind == dsl::ParseFailureKind::Committed);
  REQUIRE(out.error.expected[0] == "<end-of-input>");
}

TEST_CASE("satisfy parses predicate") {
  auto digit = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
  dsl::ParsecInput in{"7", 0};
  auto r = digit(in);
  REQUIRE(r);
  REQUIRE(*r == '7');
}

TEST_CASE("satisfy reports label") {
  auto digit = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
  dsl::ParsecInput in{"x", 0};
  auto r = digit(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.expected[0] == "digit");
}

TEST_CASE("error merge keeps furthest") {
  dsl::ParseError a{1, dsl::ParseFailureKind::Soft, {"a"}};
  dsl::ParseError b{3, dsl::ParseFailureKind::Soft, {"b"}};
  dsl::merge_error(a, b);
  REQUIRE(a.pos == 3);
  REQUIRE(a.expected[0] == "b");
}

TEST_CASE("error merge unions labels at same pos") {
  dsl::ParseError a{2, dsl::ParseFailureKind::Soft, {"a"}};
  dsl::ParseError b{2, dsl::ParseFailureKind::Committed, {"b"}};
  dsl::merge_error(a, b);
  REQUIRE(a.kind == dsl::ParseFailureKind::Committed);
  REQUIRE(a.expected.size() == 2);
}

TEST_CASE("fail_expected has location and label") {
  dsl::ParsecInput in{"abc", 2};
  auto r = dsl::fail_expected<char>(in, "x");
  REQUIRE_FALSE(r);
  REQUIRE(r.error.pos == 2);
  REQUIRE(r.error.expected[0] == "x");
}

TEST_CASE("alternative merge keeps committed at equal furthest pos") {
  auto a = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Committed, {"A"});
  });
  auto b = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<char> {
    return dsl::ExpectedResult<char>::failure(in.pos, dsl::ParseFailureKind::Soft, {"B"});
  });
  dsl::ParsecInput in{"", 0};
  auto r = (a | b)(in);
  REQUIRE_FALSE(r);
  REQUIRE(r.error.kind == dsl::ParseFailureKind::Committed);
  REQUIRE(r.error.expected[0] == "A");
}
