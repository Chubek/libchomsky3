/// @file satie-repl.cpp
/// @brief Interactive REPL for the Satie SAT solver.
///
/// Standard-library only. No readline, no Boost.
///
/// Commands:
///   :load <file>        parse a DIMACS/DSL file into the active problem
///   :engine <e>         native | dpll | cdcl  (default cdcl)
///   :solve              solve the active problem and print SAT/UNSAT
///   :model              print the satisfying model after :solve
///   :count              count models (native engine only)
///   :reset              clear the active problem
///   :status             show active engine, clause/var counts
///   :help               list commands
///   :quit | :exit       leave the REPL
///
/// Bare non-':' lines are accumulated as CNF DSL / DIMACS input.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Satie.hpp"

namespace {

constexpr const char *kBanner =
    "Satie REPL — header-only SAT solver. Type :help for commands.";

// ---- minimal syntax highlighter driven by cli/Satie.syn semantics -------
// The REPL applies ANSI escapes for: keywords, negation, operators,
// parens, comments, numbers. This mirrors the class table in Satie.syn.

const char *kReset = "\033[0m";
const char *kCyan = "\033[36m";     // keyword
const char *kYellow = "\033[33m";   // operator
const char *kRed = "\033[31m";      // negation
const char *kBlue = "\033[34m";     // paren
const char *kGray = "\033[90m";     // comment
const char *kMagenta = "\033[35m";  // number

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdent(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string highlight(const std::string &line) {
  std::ostringstream out;
  std::size_t i = 0;
  const std::size_t n = line.size();
  while (i < n) {
    char c = line[i];
    // comment: '#' or DIMACS leading 'c'
    if (c == '#') {
      out << kGray << line.substr(i) << kReset;
      break;
    }
    if (c == 'c' && (i == 0 || line[i - 1] == '\n' || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
      // only if the rest of the token is whitespace → treat as DIMACS comment line
      std::size_t j = i + 1;
      if (j >= n || std::isspace(static_cast<unsigned char>(line[j]))) {
        out << kGray << line.substr(i) << kReset;
        break;
      }
    }
    if (c == '~' || c == '!') { out << kRed << c << kReset; ++i; continue; }
    if (c == '(' || c == ')') { out << kBlue << c << kReset; ++i; continue; }
    if (c == '&' || c == '|') { out << kYellow << c << kReset; ++i; continue; }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t j = i;
      while (j < n && std::isdigit(static_cast<unsigned char>(line[j]))) ++j;
      out << kMagenta << line.substr(i, j - i) << kReset;
      i = j; continue;
    }
    if (isIdentStart(c)) {
      std::size_t j = i;
      while (j < n && isIdent(line[j])) ++j;
      std::string tok = line.substr(i, j - i);
      if (tok == "p" || tok == "cnf" || tok == "SAT" || tok == "UNSAT")
        out << kCyan << tok << kReset;
      else
        out << tok;
      i = j; continue;
    }
    out << c;
    ++i;
  }
  return out.str();
}

std::string readFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open file: " + path);
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

void printModel(const satie::Assignment &m) {
  std::cout << "model:";
  bool any = false;
  for (std::size_t v = 1; v <= m.size(); ++v) {
    satie::Value val = m.get_var(static_cast<satie::Var>(v));
    if (val == satie::Value::UNKNOWN) continue;
    any = true;
    std::cout << "\n  x" << v << " = " << satie::value_name(val);
  }
  std::cout << (any ? "\n" : " (empty)\n");
}

} // namespace

int main() {
  std::cout << kBanner << "\n";
  satie::Engine engine = satie::Engine::CDCL;
  std::vector<std::string> buffer;
  std::optional<satie::Solver> solver;
  std::optional<satie::SolverReport> lastReport;

  std::string line;
  while (std::cout << "satie> " && std::cout.flush() && std::getline(std::cin, line)) {
    // trim
    auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) continue;
    std::string trimmed = line.substr(first);
    auto last = trimmed.find_last_not_of(" \t\r\n");
    trimmed.erase(last + 1);

    if (trimmed.empty()) continue;

    if (trimmed[0] == ':') {
      // command
      std::istringstream cmd(trimmed.substr(1));
      std::string verb;
      cmd >> verb;
      if (verb == "quit" || verb == "exit" || verb == "q") {
        break;
      } else if (verb == "help" || verb == "h" || verb == "?") {
        std::cout <<
          "Commands:\n"
          "  :load <file>      parse DIMACS/DSL file\n"
          "  :engine <e>       native | dpll | cdcl\n"
          "  :solve            solve active problem\n"
          "  :model            show model from last :solve\n"
          "  :count            count models (native only)\n"
          "  :reset            clear active problem\n"
          "  :status           show engine + counts\n"
          "  :help             this message\n"
          "  :quit             exit\n";
      } else if (verb == "load") {
        std::string path;
        if (!(cmd >> path)) { std::cout << "error: :load requires a path\n"; continue; }
        try {
          std::string text = readFile(path);
          satie::CNF cnf = satie::parse_auto(text);
          solver.emplace(std::move(cnf));
          std::cout << "loaded: " << solver->problem().clause_count()
                    << " clauses, " << solver->problem().variable_count() << " vars\n";
        } catch (const std::exception &e) {
          std::cout << "load error: " << e.what() << "\n";
        }
      } else if (verb == "engine") {
        std::string e;
        if (!(cmd >> e)) { std::cout << "usage: :engine <native|dpll|cdcl>\n"; continue; }
        if (e == "native") engine = satie::Engine::Native;
        else if (e == "dpll") engine = satie::Engine::DPLL;
        else if (e == "cdcl") engine = satie::Engine::CDCL;
        else { std::cout << "unknown engine: " << e << "\n"; continue; }
        std::cout << "engine = " << satie::engine_name(engine) << "\n";
      } else if (verb == "solve") {
        if (!solver) { std::cout << "no active problem; use :load or type a CNF formula\n"; continue; }
        try {
          lastReport = solver->solve_with_report(satie::SolveOptions{engine});
          std::cout << (lastReport->result.status == satie::SolveStatus::SAT ? "SAT" : "UNSAT") << "\n";
        } catch (const std::exception &e) {
          std::cout << "solve error: " << e.what() << "\n";
        }
      } else if (verb == "model") {
        if (!lastReport) { std::cout << "run :solve first\n"; continue; }
        if (lastReport->result.status != satie::SolveStatus::SAT)
          std::cout << "no model (UNSAT)\n";
        else
          printModel(lastReport->result.assignment);
      } else if (verb == "count") {
        if (!solver) { std::cout << "no active problem\n"; continue; }
        try {
          satie::NaiveSolver ns(solver->problem());
          std::cout << "models: " << ns.count_models() << "\n";
        } catch (const std::exception &e) {
          std::cout << "count error: " << e.what() << "\n";
        }
      } else if (verb == "reset") {
        solver.reset();
        lastReport.reset();
        buffer.clear();
        std::cout << "reset\n";
      } else if (verb == "status") {
        std::cout << "engine: " << satie::engine_name(engine) << "\n";
        if (solver)
          std::cout << "clauses: " << solver->problem().clause_count()
                    << ", vars: " << solver->problem().variable_count() << "\n";
        else
          std::cout << "no active problem\n";
      } else {
        std::cout << "unknown command: :" << verb << " (try :help)\n";
      }
      continue;
    }

    // highlight for display
    std::cout << highlight(trimmed) << "\n";

    // accumulate as DSL/DIMACS; try to parse and load immediately
    buffer.push_back(trimmed);
    std::string joined;
    for (auto &b : buffer) { joined += b; joined += '\n'; }
    try {
      satie::CNF cnf = satie::parse_auto(joined);
      solver.emplace(std::move(cnf));
      std::cout << "parsed: " << solver->problem().clause_count()
                << " clauses, " << solver->problem().variable_count() << " vars\n";
    } catch (const satie::ParseError &e) {
      std::cout << "parse: " << e.what() << "\n"
                << "  (formula not yet complete or malformed)\n";
    } catch (const std::exception &e) {
      std::cout << "error: " << e.what() << "\n";
    }
  }

  std::cout << "bye.\n";
  return 0;
}
