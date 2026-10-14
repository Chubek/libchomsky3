// 10 tests for Common.hpp foundational data structures.

#include "catch_shim.hpp"
#include "Common.hpp"
using namespace satie;

TEST_CASE("make_literal encodes sign") {
  REQUIRE(make_literal(3) == 3);
  REQUIRE(make_literal(3, true) == -3);
}

TEST_CASE("literal_var and is_negate") {
  REQUIRE(literal_var(-5) == 5);
  REQUIRE(literal_var(5) == 5);
  REQUIRE(is_negated(-5));
  REQUIRE_FALSE(is_negated(5));
  REQUIRE(negate(-7) == 7);
  REQUIRE(negate(7) == -7);
}

TEST_CASE("Assignment tracks variables") {
  Assignment a(3);
  REQUIRE(a.size() == 3);
  REQUIRE(a.get_var(1) == Value::UNKNOWN);
  a.assign(1, true);
  REQUIRE(a.get_var(1) == Value::TRUE);
  REQUIRE(a.get_literal(-1) == Value::FALSE);
  REQUIRE(a.is_assigned(1));
  REQUIRE_FALSE(a.is_assigned(2));
}

TEST_CASE("Assignment grows on demand") {
  Assignment a(1);
  a.assign(5, false);
  REQUIRE(a.size() >= 5);
  REQUIRE(a.get_var(5) == Value::FALSE);
}

TEST_CASE("CNF add_clause dedups and counts") {
  CNF c;
  c.add_clause({1, 1, 2});
  REQUIRE(c.clause_count() == 1);
  REQUIRE(c.variable_count() == 2);
}

TEST_CASE("CNF empty clause detection") {
  CNF c;
  c.add_clause({0});
  REQUIRE(c.has_empty_clause());
}

TEST_CASE("CNF stores multiple clauses") {
  CNF c({{1, 2}, {1, 2}});
  REQUIRE(c.clause_count() == 2);
}

TEST_CASE("DIMACS round-trip") {
  CNF c({{1, -2}, {3}});
  std::string s = to_dimacs_string(c);
  REQUIRE(s.find("p cnf 3 2") != std::string::npos);
  std::istringstream iss(s);
  DimacsParser dp{iss};
  CNF back = dp.parse();
  REQUIRE(back.variable_count() == 3);
  REQUIRE(back.clause_count() == 2);
}

TEST_CASE("clause satisfaction and conflict helpers") {
  Clause cl{1, 2};
  Assignment unsat(2);
  REQUIRE_FALSE(is_clause_satisfied(cl, unsat));
  unsat.assign(1, false);
  unsat.assign(2, false);
  REQUIRE(is_clause_conflicting(cl, unsat));
  Assignment sat(2);
  sat.assign(1, true);
  REQUIRE(is_clause_satisfied(cl, sat));
}

TEST_CASE("SymbolTable interns uniquely") {
  SymbolTable st;
  Var a = st.intern("a");
  Var b = st.intern("b");
  Var a2 = st.intern("a");
  REQUIRE(a == a2);
  REQUIRE(a != b);
}
