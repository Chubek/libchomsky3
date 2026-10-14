// 10 tests for solver cores: Native, DPLL, CDCL.

#include "catch_shim.hpp"
#include "Satie.hpp"
using namespace satie;

TEST_CASE("CDCL trivially SAT") {
  CNF c({{1}});
  REQUIRE(solve(c, Engine::CDCL).satisfiable());
}

TEST_CASE("CDCL trivially UNSAT") {
  CNF c({{1}, {-1}});
  REQUIRE_FALSE(solve(c, Engine::CDCL).satisfiable());
}

TEST_CASE("DPLL pigeonhole SAT") {
  CNF c({{1, 2}, {-1, 3}});
  REQUIRE(solve(c, Engine::DPLL).satisfiable());
}

TEST_CASE("DPLL contradictory UNSAT") {
  CNF c({{1, 2}, {-1, -2}, {1, -2}, {-1, 2}});
  REQUIRE_FALSE(solve(c, Engine::DPLL).satisfiable());
}

TEST_CASE("Native exhaustive SAT") {
  CNF c({{1, 2}});
  REQUIRE(solve(c, Engine::Native).satisfiable());
}

TEST_CASE("Native model count") {
  CNF c({{1, 2}});
  NaiveSolver s(c);
  REQUIRE(s.count_models() == 3);
}

TEST_CASE("engines agree on random SAT instance") {
  CNF c({{1, 2, 3}, {-1, 2}, {-2, 3}, {1, -3}});
  bool cdcl = solve(c, Engine::CDCL).satisfiable();
  bool dpll = solve(c, Engine::DPLL).satisfiable();
  bool naive = solve(c, Engine::Native).satisfiable();
  REQUIRE(cdcl == dpll);
  REQUIRE(dpll == naive);
}

TEST_CASE("Solver facade SAT report") {
  Solver s(CNF({{1, 2}, {2, 3}}));
  SolverReport r = s.solve_with_report({Engine::CDCL});
  REQUIRE(r.result.satisfiable());
  REQUIRE(r.engine == Engine::CDCL);
  REQUIRE(r.statistics.cdcl.has_value());
}

TEST_CASE("empty CNF is SAT") {
  CNF c;
  REQUIRE(solve(c, Engine::CDCL).satisfiable());
  REQUIRE(solve(c, Engine::DPLL).satisfiable());
}

TEST_CASE("parse_auto dispatches DIMACS vs DSL") {
  CNF dimacs = parse_auto("p cnf 2 1\n1 2 0\n");
  REQUIRE(dimacs.clause_count() == 1);
  CNF dsl = parse_auto("(a | b)");
  REQUIRE(dsl.clause_count() == 1);
}
