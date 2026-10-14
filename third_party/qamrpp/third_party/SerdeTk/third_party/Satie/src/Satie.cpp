#include "Satie.hpp"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace satie
{

static bool looks_like_dimacs (const std::string &text)
{
  std::istringstream in (text);
  std::string line;
  while (std::getline (in, line))
    {
      auto first = line.find_first_not_of (" \t\r\n");
      if (first == std::string::npos)
        continue;
      char lead = line[first];
      if (lead == 'c')
        continue;
      if (lead == 'p')
        return true;
      if (lead == 'p')
        return true;
      return false;
    }
  return false;
}

static CNF parse_dimacs_text (const std::string &text)
{
  std::istringstream in (text);
  return parse_dimacs (in);
}

SolveResult Solver::solve (Engine engine) const
{
  switch (engine)
    {
    case Engine::Native:
      return solve_naive (cnf_);
    case Engine::DPLL:
      return solve_dpll (cnf_);
    case Engine::CDCL:
      return solve_cdcl (cnf_);
    }
  std::abort ();
}

SolverReport Solver::solve_with_report (const SolveOptions &options) const
{
  SolverReport report{};
  report.engine = options.engine;

  switch (options.engine)
    {
    case Engine::Native:
      {
        NaiveSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.native = solver.statistics ();
        break;
      }
    case Engine::DPLL:
      {
        DPLLSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.dpll = solver.statistics ();
        break;
      }
    case Engine::CDCL:
      {
        CDCLSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.cdcl = solver.statistics ();
        break;
      }
    }

  return report;
}

bool Solver::satisfiable (Engine engine) const { return solve (engine).satisfiable (); }

CNF parse_auto (const std::string &text)
{
  return parse (text, ParseFormat::Auto);
}

CNF parse_auto_file (const std::string &path)
{
  return parse_file (path, ParseFormat::Auto);
}

CNF parse (const std::string &text, ParseFormat format)
{
  switch (format)
    {
    case ParseFormat::Auto:
      return looks_like_dimacs (text) ? parse_dimacs_text (text) : parse_cnf (text);
    case ParseFormat::CNF:
      return parse_cnf (text);
    case ParseFormat::DIMACS:
      return parse_dimacs_text (text);
    }
  std::abort ();
}

CNF parse_file (const std::string &path, ParseFormat format)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("failed to open input file: " + path);

  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return parse (buffer.str (), format);
}

SolveResult solve (const CNF &cnf, Engine engine)
{
  Solver solver (cnf);
  return solver.solve (engine);
}

SolveResult solve (const std::string &input, Engine engine)
{
  return solve (parse_auto (input), engine);
}

SolverReport solve_with_report (const CNF &cnf, const SolveOptions &options)
{
  Solver solver (cnf);
  return solver.solve_with_report (options);
}

} // namespace satie
