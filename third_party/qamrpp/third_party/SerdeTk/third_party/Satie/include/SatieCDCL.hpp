#pragma once

#include <algorithm>
#include <cstdint>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Common.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

class CDCLSolver
{
public:
  struct Statistics
  {
    std::uint64_t decisions = 0;
    std::uint64_t propagations = 0;
    std::uint64_t conflicts = 0;
    std::uint64_t learned_clauses = 0;
    std::uint64_t restarts = 0;
    std::uint64_t backjumps = 0;
  };

  CDCLSolver () = default;
  explicit CDCLSolver (CNF cnf) { load (std::move (cnf)); }

  void load (CNF cnf)
  {
    original_ = dimacs_to_cnf (cnf_to_dimacs (cnf));
    initialize ();
  }

  const Statistics &stats () const { return stats_; }
  const Statistics &statistics () const { return stats_; }

  SolveResult solve ()
  {
    initialize ();
    int initial = propagate_full ();
    if (initial != -1)
      return { SolveStatus::UNSAT, Assignment (original_.variable_count ()) };

    while (true)
      {
        int conflict = propagate_full ();
        if (conflict != -1)
          {
            ++stats_.conflicts;
            if (decision_level_ == 0)
              return { SolveStatus::UNSAT, Assignment (original_.variable_count ()) };
            auto [learned, backtrack_level] = analyze_conflict (conflict);
            add_learned_clause (std::move (learned));
            backjump (backtrack_level);
            maybe_restart ();
            continue;
          }

        if (all_variables_assigned ())
          return { SolveStatus::SAT, build_assignment () };

        Var decision = pick_branch_variable ();
        ++decision_level_;
        ++stats_.decisions;
        assign_literal (make_literal (decision, false), decision_level_, -1);
      }
  }

private:
  struct TrailEntry
  {
    Lit literal = 0;
    int level = 0;
    int antecedent = -1;
  };
  struct InternalAssignment
  {
    std::vector<Value> assignments;
    std::vector<int> levels;
    std::vector<int> reasons;
  };

  void initialize ()
  {
    clauses_ = original_.clauses ();
    assignment_.assignments.assign (original_.variable_count () + 1, Value::UNKNOWN);
    assignment_.levels.assign (original_.variable_count () + 1, 0);
    assignment_.reasons.assign (original_.variable_count () + 1, -1);
    activity_.assign (original_.variable_count () + 1, 0.0);
    trail_.clear ();
    decision_level_ = 0;
    conflict_budget_ = restart_interval_;
    stats_ = {};
  }

  void maybe_restart ()
  {
    if (stats_.conflicts < conflict_budget_)
      return;
    restart_to_root ();
    conflict_budget_ += restart_interval_;
    ++stats_.restarts;
  }

  void restart_to_root ()
  {
    while (!trail_.empty () && trail_.back ().level > 0)
      {
        Var v = literal_var (trail_.back ().literal);
        assignment_.assignments[v] = Value::UNKNOWN;
        assignment_.levels[v] = 0;
        assignment_.reasons[v] = -1;
        trail_.pop_back ();
      }
    decision_level_ = 0;
  }

  bool literal_true (Lit lit) const
  {
    Value v = assignment_.assignments[literal_var (lit)];
    if (v == Value::UNKNOWN)
      return false;
    return lit > 0 ? v == Value::TRUE : v == Value::FALSE;
  }
  bool literal_false (Lit lit) const
  {
    Value v = assignment_.assignments[literal_var (lit)];
    if (v == Value::UNKNOWN)
      return false;
    return lit > 0 ? v == Value::FALSE : v == Value::TRUE;
  }
  bool literal_unknown (Lit lit) const
  {
    return assignment_.assignments[literal_var (lit)] == Value::UNKNOWN;
  }

  bool assign_literal (Lit lit, int level, int reason)
  {
    Var v = literal_var (lit);
    Value next = lit > 0 ? Value::TRUE : Value::FALSE;
    if (assignment_.assignments[v] != Value::UNKNOWN)
      return assignment_.assignments[v] == next;
    assignment_.assignments[v] = next;
    assignment_.levels[v] = level;
    assignment_.reasons[v] = reason;
    trail_.push_back ({ lit, level, reason });
    return true;
  }

  int propagate_full ()
  {
    bool changed = true;
    while (changed)
      {
        changed = false;
        for (std::size_t i = 0; i < clauses_.size (); ++i)
          {
            const Clause &clause = clauses_[i];
            bool satisfied = false;
            int unknowns = 0;
            Lit candidate = 0;
            for (Lit lit : clause)
              {
                if (literal_true (lit))
                  {
                    satisfied = true;
                    break;
                  }
                if (literal_unknown (lit))
                  {
                    ++unknowns;
                    candidate = lit;
                  }
              }
            if (satisfied)
              continue;
            if (unknowns == 0)
              return static_cast<int> (i);
            if (unknowns == 1 && !literal_false (candidate))
              {
                if (!assign_literal (candidate, decision_level_, static_cast<int> (i)))
                  return static_cast<int> (i);
                ++stats_.propagations;
                changed = true;
              }
          }
      }
    return -1;
  }

  std::pair<Clause, int> analyze_conflict (int clause_index)
  {
    Clause learned = clauses_[clause_index];
    int current_level_count = count_current_level (learned);
    std::size_t trail_index = trail_.size ();

    while (current_level_count > 1 && trail_index > 0)
      {
        Lit pivot = trail_[--trail_index].literal;
        Var pv = literal_var (pivot);
        if (assignment_.levels[pv] != decision_level_ || assignment_.reasons[pv] < 0)
          continue;

        Clause resolved;
        for (Lit lit : learned)
          if (literal_var (lit) != pv)
            resolved.push_back (lit);
        for (Lit lit : clauses_[assignment_.reasons[pv]])
          if (literal_var (lit) != pv
              && std::find (resolved.begin (), resolved.end (), lit) == resolved.end ())
            resolved.push_back (lit);
        learned = std::move (resolved);
        current_level_count = count_current_level (learned);
      }

    int backtrack_level = 0;
    for (Lit lit : learned)
      {
        int lvl = assignment_.levels[literal_var (lit)];
        if (lvl != decision_level_)
          backtrack_level = std::max (backtrack_level, lvl);
        activity_[literal_var (lit)] += 1.0;
      }
    return { learned, backtrack_level };
  }

  int count_current_level (const Clause &clause) const
  {
    int count = 0;
    std::unordered_set<Var> seen;
    for (Lit lit : clause)
      if (assignment_.levels[literal_var (lit)] == decision_level_ && seen.insert (literal_var (lit)).second)
        ++count;
    return count;
  }

  void add_learned_clause (Clause clause)
  {
    clauses_.push_back (std::move (clause));
    ++stats_.learned_clauses;
  }

  void backjump (int level)
  {
    ++stats_.backjumps;
    while (!trail_.empty () && trail_.back ().level > level)
      {
        Var v = literal_var (trail_.back ().literal);
        assignment_.assignments[v] = Value::UNKNOWN;
        assignment_.levels[v] = 0;
        assignment_.reasons[v] = -1;
        trail_.pop_back ();
      }
    decision_level_ = level;
  }

  bool all_variables_assigned () const
  {
    for (Var v = 1; v <= static_cast<Var> (original_.variable_count ()); ++v)
      if (assignment_.assignments[v] == Value::UNKNOWN)
        return false;
    return true;
  }

  Var pick_branch_variable () const
  {
    double best = -1.0;
    Var chosen = 0;
    for (Var v = 1; v <= static_cast<Var> (original_.variable_count ()); ++v)
      if (assignment_.assignments[v] == Value::UNKNOWN && activity_[v] >= best)
        {
          best = activity_[v];
          chosen = v;
        }
    return chosen == 0 ? 1 : chosen;
  }

  Assignment build_assignment () const
  {
    Assignment out (original_.variable_count ());
    for (Var v = 1; v <= static_cast<Var> (original_.variable_count ()); ++v)
      {
        if (assignment_.assignments[v] == Value::TRUE)
          out.assign (v, true);
        else if (assignment_.assignments[v] == Value::FALSE)
          out.assign (v, false);
      }
    return out;
  }

  CNF original_{};
  ClauseList clauses_{};
  InternalAssignment assignment_{};
  std::vector<TrailEntry> trail_{};
  std::vector<double> activity_{};
  int decision_level_ = 0;
  std::uint64_t restart_interval_ = 128;
  std::uint64_t conflict_budget_ = restart_interval_;
  Statistics stats_{};
};

inline SolveResult solve_cdcl (const CNF &cnf)
{
  CDCLSolver solver (cnf);
  return solver.solve ();
}
inline bool is_satisfiable_cdcl (const CNF &cnf) { return solve_cdcl (cnf).satisfiable (); }

} // namespace satie
