/// @file basic_solve.cpp
/// @brief Initialize the solver, add clauses, print the result.

#include <iostream>
#include "Satie.hpp"

int main() {
  // Construct a CNF: (x1 | x2) & (~x1 | x3) & (~x2 | ~x3)
  satie::CNF cnf({{1, 2}, {-1, 3}, {-2, -3}});

  satie::Solver solver(std::move(cnf));
  satie::SolverReport report = solver.solve_with_report({satie::Engine::CDCL});

  std::cout << "status: "
            << (report.result.satisfiable() ? "SAT" : "UNSAT") << "\n";
  std::cout << "engine: " << satie::engine_name(report.engine) << "\n";

  if (report.result.satisfiable()) {
    std::cout << "model: " << report.result.assignment.dump() << "\n";
  }

  // Per-engine statistics.
  if (report.statistics.cdcl) {
    std::cout << "conflicts: " << report.statistics.cdcl->conflicts << "\n";
  }
  return report.result.satisfiable() ? 0 : 1;
}
