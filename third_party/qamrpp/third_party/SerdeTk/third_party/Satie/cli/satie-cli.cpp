/// @file satie-cli.cpp
/// @brief Non-interactive batch driver for Satie.
///
/// Usage:
///   satie-cli [--engine native|dpll|cdcl] [--model] <file>
///   cat formula.cnf | satie-cli [--engine cdcl] -
///
/// Exits 0 on SAT, 1 on UNSAT, 2 on error.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Satie.hpp"

namespace {

void usage() {
  std::cerr <<
    "usage: satie-cli [--engine native|dpll|cdcl] [--model] <file|->\n"
    "  --engine   solver backend (default: cdcl)\n"
    "  --model    print the satisfying model on SAT\n"
    "  -          read formula from stdin\n";
}

std::string readAll(std::istream &in) {
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

} // namespace

int main(int argc, char **argv) {
  satie::Engine engine = satie::Engine::CDCL;
  bool showModel = false;
  std::string path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--engine") {
      if (++i >= argc) { usage(); return 2; }
      std::string e = argv[i];
      if (e == "native") engine = satie::Engine::Native;
      else if (e == "dpll") engine = satie::Engine::DPLL;
      else if (e == "cdcl") engine = satie::Engine::CDCL;
      else { std::cerr << "unknown engine: " << e << "\n"; return 2; }
    } else if (arg == "--model") {
      showModel = true;
    } else if (arg == "-h" || arg == "--help") {
      usage(); return 0;
    } else if (arg == "-") {
      path = "-";
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n"; usage(); return 2;
    } else {
      path = arg;
    }
  }

  if (path.empty()) { usage(); return 2; }

  std::string text;
  try {
    if (path == "-") {
      text = readAll(std::cin);
    } else {
      std::ifstream in(path);
      if (!in) { std::cerr << "error: cannot open " << path << "\n"; return 2; }
      text = readAll(in);
    }
    satie::CNF cnf = satie::parse_auto(text);
    satie::Solver solver(std::move(cnf));
    satie::SolveResult r = solver.solve(engine);
    if (r.status == satie::SolveStatus::SAT) {
      std::cout << "SAT\n";
      if (showModel) {
        for (std::size_t v = 1; v <= r.assignment.size(); ++v) {
          satie::Value val = r.assignment.get_var(static_cast<satie::Var>(v));
          if (val == satie::Value::UNKNOWN) continue;
          std::cout << "x" << v << " = " << satie::value_name(val) << "\n";
        }
      }
      return 0;
    }
    std::cout << "UNSAT\n";
    return 1;
  } catch (const satie::ParseError &e) {
    std::cerr << "parse error: " << e.what() << "\n";
    return 2;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
}
