/// @file build_ast.cpp
/// @brief Demonstrate direct interaction with DSLtk.hpp objects:
///        build an AST, convert to a ProblemDAG, emit Graphviz DOT,
///        and solve via the CNF it represents.

#include <iostream>
#include "Satie.hpp"
#include "DSLtk.hpp"

int main() {
  // 1. Build a small CNF through the API.
  satie::CNF cnf({{1, 2}, {-1, 3}});

  // 2. Convert to the dsl::ASTNode DAG (cnf -> clause -> literal/var/sign).
  satie::ProblemDAG dag = satie::cnf_to_dag(cnf);
  std::cout << "AST dump:\n" << dag.root.dump() << "\n\n";

  // 3. Emit Graphviz DOT for visualization.
  std::cout << "--- DOT ---\n" << satie::dag_to_dot(dag);

  // 4. Hand-build an AST node directly with dsl::node / dsl::leaf to show
  //    the combinatory toolkit surface available to DSL authors.
  auto custom = dsl::node<"formula">(
      dsl::leaf<"comment">("built from DSLtk.hpp primitives"));
  std::cout << "\ncustom AST: " << custom.dump() << "\n";

  // 5. Solve the original CNF.
  satie::Solver solver(cnf);
  std::cout << "satisfiable: "
            << (solver.satisfiable(satie::Engine::CDCL) ? "yes" : "no") << "\n";
  return 0;
}
