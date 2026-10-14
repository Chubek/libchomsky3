#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "Common.hpp"
#include "SatieCDCL.hpp"
#include "SatieDPLL.hpp"
#include "SatieNative.hpp"

namespace satie
{

enum class Engine : std::uint8_t
{
  Native,
  DPLL,
  CDCL
};

enum class ParseFormat : std::uint8_t
{
  Auto,
  CNF,
  DIMACS
};

inline std::string_view engine_name (Engine engine)
{
  switch (engine)
    {
    case Engine::Native:
      return "native";
    case Engine::DPLL:
      return "dpll";
    case Engine::CDCL:
      return "cdcl";
    }
  return "unknown";
}

struct SolveOptions
{
  Engine engine = Engine::CDCL;
};

struct EngineStatistics
{
  std::optional<NaiveSolver::Statistics> native;
  std::optional<DPLLSolver::Statistics> dpll;
  std::optional<CDCLSolver::Statistics> cdcl;
};

struct SolverReport
{
  SolveResult result{};
  Engine engine = Engine::CDCL;
  EngineStatistics statistics{};
};

class Solver
{
public:
  Solver () = default;
  explicit Solver (CNF cnf) : cnf_ (std::move (cnf)) {}

  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const { return cnf_; }

  SolveResult solve (Engine engine = Engine::CDCL) const;
  SolverReport solve_with_report (const SolveOptions &options = {}) const;
  bool satisfiable (Engine engine = Engine::CDCL) const;

private:
  CNF cnf_{};
};

CNF parse_auto (const std::string &text);
CNF parse_auto_file (const std::string &path);
CNF parse (const std::string &text, ParseFormat format);
CNF parse_file (const std::string &path, ParseFormat format);

SolveResult solve (const CNF &cnf, Engine engine = Engine::CDCL);
SolveResult solve (const std::string &input, Engine engine = Engine::CDCL);
SolverReport solve_with_report (const CNF &cnf, const SolveOptions &options = {});

} // namespace satie
