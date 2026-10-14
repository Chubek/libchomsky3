#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Common.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

class DPLLSolver
{
public:
  struct Statistics
  {
    std::uint64_t recursive_calls = 0;
    std::uint64_t decisions = 0;
    std::uint64_t propagations = 0;
    std::uint64_t pure_literal_eliminations = 0;
    std::uint64_t backtracks = 0;
    std::uint64_t conflicts = 0;
  };

  DPLLSolver () = default;
  explicit DPLLSolver (CNF cnf) { load (std::move (cnf)); }

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
    Assignment assignment (cnf_.variable_count ());
    if (search (assignment))
      return { SolveStatus::SAT, model_ };
    return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
  }

private:
  bool search (Assignment &assignment)
  {
    ++stats_.recursive_calls;
    if (!propagate_units (assignment))
      {
        ++stats_.conflicts;
        return false;
      }
    eliminate_pure_literals (assignment);
    if (has_conflict (cnf_, assignment))
      {
        ++stats_.conflicts;
        return false;
      }
    if (is_formula_satisfied (cnf_, assignment))
      {
        model_ = assignment;
        return true;
      }

    std::optional<Var> branch = choose_variable (assignment);
    if (!branch)
      return false;

    ++stats_.decisions;
    Assignment left = assignment;
    left.assign (*branch, true);
    if (search (left))
      return true;

    ++stats_.backtracks;
    Assignment right = assignment;
    right.assign (*branch, false);
    if (search (right))
      return true;

    ++stats_.backtracks;
    return false;
  }

  bool propagate_units (Assignment &assignment)
  {
    bool changed = true;
    while (changed)
      {
        changed = false;
        for (const Clause &clause : cnf_.clauses ())
          {
            bool satisfied = false;
            Lit unassigned = 0;
            std::size_t unassigned_count = 0;
            for (Lit lit : clause)
              {
                Value v = assignment.get_literal (lit);
                if (v == Value::TRUE)
                  {
                    satisfied = true;
                    break;
                  }
                if (v == Value::UNKNOWN)
                  {
                    unassigned = lit;
                    ++unassigned_count;
                  }
              }
            if (satisfied)
              continue;
            if (unassigned_count == 0)
              return false;
            if (unassigned_count == 1)
              {
                if (!assignment.assign_literal (unassigned))
                  return false;
                ++stats_.propagations;
                changed = true;
              }
          }
      }
    return true;
  }

  void eliminate_pure_literals (Assignment &assignment)
  {
    std::unordered_map<Var, int> polarity;
    for (const Clause &clause : cnf_.clauses ())
      {
        if (is_clause_satisfied (clause, assignment))
          continue;
        for (Lit lit : clause)
          {
            Var v = literal_var (lit);
            if (assignment.is_assigned (v))
              continue;
            int p = lit > 0 ? 1 : -1;
            auto it = polarity.find (v);
            if (it == polarity.end ())
              polarity[v] = p;
            else if (it->second != p)
              it->second = 0;
          }
      }
    for (const auto &[var, pol] : polarity)
      if (pol != 0 && !assignment.is_assigned (var))
        {
          assignment.assign (var, pol > 0);
          ++stats_.pure_literal_eliminations;
        }
  }

  std::optional<Var> choose_variable (const Assignment &assignment) const
  {
    std::unordered_map<Var, std::size_t> frequency;
    for (const Clause &clause : cnf_.clauses ())
      {
        if (is_clause_satisfied (clause, assignment))
          continue;
        for (Lit lit : clause)
          if (!assignment.is_assigned (literal_var (lit)))
            ++frequency[literal_var (lit)];
      }
    Var best = 0;
    std::size_t best_score = 0;
    for (const auto &[var, score] : frequency)
      if (score > best_score)
        {
          best = var;
          best_score = score;
        }
    if (best == 0)
      return std::nullopt;
    return best;
  }

  CNF cnf_{};
  Assignment model_{};
  Statistics stats_{};
};

inline SolveResult solve_dpll (const CNF &cnf)
{
  DPLLSolver solver (cnf);
  return solver.solve ();
}
inline bool is_satisfiable_dpll (const CNF &cnf) { return solve_dpll (cnf).satisfiable (); }

} // namespace satie
