#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "Common.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

class NaiveSolver
{
public:
  struct Statistics
  {
    std::uint64_t assignments_tested = 0;
    std::uint64_t recursive_calls = 0;
    std::uint64_t conflicts = 0;
    std::uint64_t satisfiable_leafs = 0;
    std::uint64_t unsatisfiable_leafs = 0;
  };

  NaiveSolver () = default;
  explicit NaiveSolver (CNF cnf) { load (std::move (cnf)); }

  void load (CNF cnf)
  {
    cnf_ = dimacs_to_cnf (cnf_to_dimacs (cnf));
    model_ = Assignment (cnf_.variable_count ());
    stats_ = {};
  }

  const Statistics &statistics () const { return stats_; }

  SolveResult solve ()
  {
    stats_ = {};
    model_ = Assignment (cnf_.variable_count ());
    if (cnf_.has_empty_clause ())
      return { SolveStatus::UNSAT, model_ };
    Assignment partial (cnf_.variable_count ());
    return dfs (1, partial) ? SolveResult{ SolveStatus::SAT, model_ }
                            : SolveResult{ SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
  }

  std::uint64_t count_models ()
  {
    stats_ = {};
    if (cnf_.variable_count () >= 64)
      throw std::overflow_error ("naive model counter exceeds uint64 search space");
    Assignment partial (cnf_.variable_count ());
    return count_dfs (1, partial);
  }

private:
  bool dfs (Var variable, Assignment &assignment)
  {
    ++stats_.recursive_calls;
    if (has_conflict (cnf_, assignment))
      {
        ++stats_.conflicts;
        return false;
      }
    if (variable > static_cast<Var> (cnf_.variable_count ()))
      {
        ++stats_.assignments_tested;
        if (is_formula_satisfied (cnf_, assignment))
          {
            ++stats_.satisfiable_leafs;
            model_ = assignment;
            return true;
          }
        ++stats_.unsatisfiable_leafs;
        return false;
      }

    assignment.assign (variable, true);
    if (dfs (variable + 1, assignment))
      return true;
    assignment.unassign (variable);

    assignment.assign (variable, false);
    if (dfs (variable + 1, assignment))
      return true;
    assignment.unassign (variable);

    return false;
  }

  std::uint64_t count_dfs (Var variable, Assignment &assignment)
  {
    ++stats_.recursive_calls;
    if (has_conflict (cnf_, assignment))
      {
        ++stats_.conflicts;
        return 0;
      }
    if (variable > static_cast<Var> (cnf_.variable_count ()))
      {
        ++stats_.assignments_tested;
        if (is_formula_satisfied (cnf_, assignment))
          {
            ++stats_.satisfiable_leafs;
            return 1;
          }
        ++stats_.unsatisfiable_leafs;
        return 0;
      }

    std::uint64_t total = 0;
    assignment.assign (variable, true);
    total += count_dfs (variable + 1, assignment);
    assignment.unassign (variable);
    assignment.assign (variable, false);
    total += count_dfs (variable + 1, assignment);
    assignment.unassign (variable);
    return total;
  }

  CNF cnf_{};
  Assignment model_{};
  Statistics stats_{};
};

inline SolveResult solve_naive (const CNF &cnf)
{
  NaiveSolver solver (cnf);
  return solver.solve ();
}
inline bool is_satisfiable_naive (const CNF &cnf) { return solve_naive (cnf).satisfiable (); }
inline std::uint64_t count_models_naive (const CNF &cnf)
{
  NaiveSolver solver (cnf);
  return solver.count_models ();
}

} // namespace satie
