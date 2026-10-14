// 10 tests for DSLtk.hpp / CNF DSL parser.

#include "catch_shim.hpp"
#include "Common.hpp"
using namespace satie;

TEST_CASE("parse single clause") {
  CNF c = parse_cnf("(a | b)");
  REQUIRE(c.clause_count() == 1);
  REQUIRE(c.variable_count() == 2);
}

TEST_CASE("parse conjunction of clauses") {
  CNF c = parse_cnf("(a | b) & (~a | c)");
  REQUIRE(c.clause_count() == 2);
  REQUIRE(c.variable_count() == 3);
}

TEST_CASE("negation operators ~ and !") {
  CNF c = parse_cnf("(!a | b) & (~b)");
  REQUIRE(c.clause_count() == 2);
}

TEST_CASE("underscore identifiers") {
  CNF c = parse_cnf("(flag_1 | _flag2)");
  REQUIRE(c.variable_count() == 2);
}

TEST_CASE("whitespace tolerant") {
  CNF c = parse_cnf("   (  a   |   b  )   &   (  c  )   ");
  REQUIRE(c.clause_count() == 2);
}

TEST_CASE("parse error on missing operator") {
  REQUIRE_THROWS_AS(parse_cnf("(a | b) (c | d)"), ParseError);
}

TEST_CASE("parse error on bare literal") {
  REQUIRE_THROWS_AS(parse_cnf("a | b"), ParseError);
}

TEST_CASE("unit clause") {
  CNF c = parse_cnf("(x)");
  REQUIRE(c.clause_count() == 1);
  REQUIRE(c.clauses()[0].size() == 1);
}

TEST_CASE("AST round-trip via cnf_to_dag") {
  CNF c = parse_cnf("(a | ~b)");
  ProblemDAG dag = cnf_to_dag(c);
  std::string dot = dag_to_dot(dag);
  REQUIRE(dot.find("digraph SatieCNF") != std::string::npos);
  REQUIRE(dot.find("clause") != std::string::npos);
}

TEST_CASE("DIMACS parse via DimacsParser") {
  std::istringstream in("p cnf 2 2\n1 2 0\n-1 -2 0\n");
  CNF c = DimacsParser(in).parse();
  REQUIRE(c.variable_count() == 2);
  REQUIRE(c.clause_count() == 2);
}
