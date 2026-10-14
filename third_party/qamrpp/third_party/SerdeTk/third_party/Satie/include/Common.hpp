#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DSLtk.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

using Var = std::int32_t;
using Lit = std::int32_t;
using Clause = std::vector<Lit>;
using ClauseList = std::vector<Clause>;

constexpr Lit make_literal (Var var, bool negated = false)
{
  return negated ? -var : var;
}
constexpr Var literal_var (Lit lit) { return lit < 0 ? -lit : lit; }
constexpr bool is_negated (Lit lit) { return lit < 0; }
constexpr Lit negate (Lit lit) { return -lit; }

enum class Value : std::uint8_t
{
  UNKNOWN = 0,
  TRUE = 1,
  FALSE = 2
};

inline std::string value_name (Value v)
{
  switch (v)
    {
    case Value::TRUE:
      return "TRUE";
    case Value::FALSE:
      return "FALSE";
    default:
      return "UNKNOWN";
    }
}

class Assignment
{
public:
  Assignment () = default;
  explicit Assignment (std::size_t variable_count)
      : values_ (variable_count + 1, Value::UNKNOWN)
  {
  }

  void resize (std::size_t variable_count)
  {
    values_.resize (variable_count + 1, Value::UNKNOWN);
  }
  std::size_t size () const { return values_.empty () ? 0 : values_.size () - 1; }

  Value get_var (Var v) const
  {
    if (v <= 0 || static_cast<std::size_t> (v) >= values_.size ())
      return Value::UNKNOWN;
    return values_[static_cast<std::size_t> (v)];
  }

  Value get_literal (Lit lit) const
  {
    Value v = get_var (literal_var (lit));
    if (v == Value::UNKNOWN)
      return Value::UNKNOWN;
    return lit > 0 ? v : (v == Value::TRUE ? Value::FALSE : Value::TRUE);
  }

  bool assign (Var v, bool value)
  {
    if (v <= 0)
      return false;
    if (static_cast<std::size_t> (v) >= values_.size ())
      resize (static_cast<std::size_t> (v));
    Value next = value ? Value::TRUE : Value::FALSE;
    Value current = get_var (v);
    if (current != Value::UNKNOWN)
      return current == next;
    values_[static_cast<std::size_t> (v)] = next;
    return true;
  }

  bool assign_literal (Lit lit) { return assign (literal_var (lit), lit > 0); }
  void unassign (Var v)
  {
    if (v > 0 && static_cast<std::size_t> (v) < values_.size ())
      values_[static_cast<std::size_t> (v)] = Value::UNKNOWN;
  }
  bool is_assigned (Var v) const { return get_var (v) != Value::UNKNOWN; }

  std::string dump () const
  {
    std::ostringstream oss;
    oss << "Assignment{";
    bool first = true;
    for (std::size_t i = 1; i < values_.size (); ++i)
      {
        if (values_[i] == Value::UNKNOWN)
          continue;
        if (!first)
          oss << ", ";
        first = false;
        oss << "x" << i << "=" << (values_[i] == Value::TRUE ? "true" : "false");
      }
    oss << "}";
    return oss.str ();
  }

private:
  std::vector<Value> values_;
};

struct ParseDiagnostic
{
  std::size_t line = 1;
  std::size_t column = 1;
  std::string message;
};

class ParseError : public std::runtime_error
{
public:
  ParseError (std::size_t line, std::size_t column, const std::string &message)
      : std::runtime_error (format (line, column, message)), diagnostic{ line, column, message }
  {
  }
  ParseDiagnostic diagnostic;

private:
  static std::string format (std::size_t line, std::size_t column, const std::string &message)
  {
    std::ostringstream oss;
    oss << "parse error at " << line << ':' << column << ": " << message;
    return oss.str ();
  }
};

class CNF
{
public:
  CNF () = default;
  explicit CNF (ClauseList clauses) : clauses_ (std::move (clauses)) { normalize (); }

  void add_clause (Clause clause)
  {
    normalize_clause (clause);
    if (clause.empty ())
      has_empty_clause_ = true;
    for (Lit lit : clause)
      variable_count_ = std::max<std::size_t> (variable_count_, literal_var (lit));
    clauses_.push_back (std::move (clause));
  }

  void set_declared_variable_count (std::size_t n)
  {
    declared_variable_count_ = n;
    variable_count_ = std::max (variable_count_, n);
  }

  const ClauseList &clauses () const { return clauses_; }
  std::size_t variable_count () const { return variable_count_; }
  std::size_t declared_variable_count () const { return declared_variable_count_; }
  std::size_t clause_count () const { return clauses_.size (); }
  bool empty () const { return clauses_.empty (); }
  bool has_empty_clause () const { return has_empty_clause_; }

  void normalize ()
  {
    variable_count_ = declared_variable_count_;
    has_empty_clause_ = false;
    for (Clause &clause : clauses_)
      {
        normalize_clause (clause);
        if (clause.empty ())
          has_empty_clause_ = true;
        for (Lit lit : clause)
          variable_count_ = std::max<std::size_t> (variable_count_, literal_var (lit));
      }
  }

  std::string dump () const
  {
    std::ostringstream oss;
    oss << "CNF(";
    for (std::size_t i = 0; i < clauses_.size (); ++i)
      {
        if (i != 0)
          oss << " & ";
        oss << '(';
        for (std::size_t j = 0; j < clauses_[i].size (); ++j)
          {
            if (j != 0)
              oss << " | ";
            if (clauses_[i][j] < 0)
              oss << '~';
            oss << 'x' << literal_var (clauses_[i][j]);
          }
        oss << ')';
      }
    oss << ')';
    return oss.str ();
  }

private:
  static void normalize_clause (Clause &clause)
  {
    clause.erase (std::remove (clause.begin (), clause.end (), 0), clause.end ());
    std::sort (clause.begin (), clause.end (), [] (Lit a, Lit b) {
      Var av = literal_var (a), bv = literal_var (b);
      return av == bv ? a < b : av < bv;
    });
    clause.erase (std::unique (clause.begin (), clause.end ()), clause.end ());
  }

  ClauseList clauses_;
  std::size_t variable_count_ = 0;
  std::size_t declared_variable_count_ = 0;
  bool has_empty_clause_ = false;
};

struct DIMACS
{
  std::size_t variables = 0;
  ClauseList clauses;
};

inline DIMACS cnf_to_dimacs (const CNF &cnf)
{
  return DIMACS{ cnf.variable_count (), cnf.clauses () };
}

inline CNF dimacs_to_cnf (DIMACS dimacs)
{
  CNF cnf (std::move (dimacs.clauses));
  cnf.set_declared_variable_count (dimacs.variables);
  return cnf;
}

inline std::string to_dimacs_string (const CNF &cnf)
{
  std::ostringstream oss;
  oss << "p cnf " << cnf.variable_count () << ' ' << cnf.clause_count () << '\n';
  for (const Clause &clause : cnf.clauses ())
    {
      for (Lit lit : clause)
        oss << lit << ' ';
      oss << "0\n";
    }
  return oss.str ();
}

inline bool is_clause_satisfied (const Clause &clause, const Assignment &assignment)
{
  for (Lit lit : clause)
    if (assignment.get_literal (lit) == Value::TRUE)
      return true;
  return false;
}

inline bool is_clause_conflicting (const Clause &clause, const Assignment &assignment)
{
  for (Lit lit : clause)
    {
      Value v = assignment.get_literal (lit);
      if (v == Value::TRUE || v == Value::UNKNOWN)
        return false;
    }
  return true;
}

inline bool is_formula_satisfied (const CNF &cnf, const Assignment &assignment)
{
  for (const Clause &clause : cnf.clauses ())
    if (!is_clause_satisfied (clause, assignment))
      return false;
  return !cnf.has_empty_clause ();
}
inline bool has_conflict (const CNF &cnf, const Assignment &assignment)
{
  if (cnf.has_empty_clause ())
    return true;
  for (const Clause &clause : cnf.clauses ())
    if (is_clause_conflicting (clause, assignment))
      return true;
  return false;
}

enum class SolveStatus
{
  SAT,
  UNSAT,
  UNKNOWN
};

struct SolveResult
{
  SolveStatus status = SolveStatus::UNKNOWN;
  Assignment assignment{};
  [[nodiscard]] bool satisfiable () const { return status == SolveStatus::SAT; }
  [[nodiscard]] bool unsatisfiable () const { return status == SolveStatus::UNSAT; }
};

class DimacsParser
{
public:
  explicit DimacsParser (std::istream &input) : input_ (input) {}

  CNF parse ()
  {
    std::string line;
    std::size_t line_no = 0;
    std::optional<std::size_t> declared_vars;
    std::optional<std::size_t> declared_clauses;
    ClauseList clauses;
    Clause current;

    while (std::getline (input_, line))
      {
        ++line_no;
        std::size_t first = first_non_ws (line);
        if (first == std::string::npos || line[first] == 'c')
          continue;
        if (line[first] == 'p')
          {
            if (declared_vars)
              throw ParseError (line_no, first + 1, "duplicate problem line");
            std::stringstream ss (line.substr (first));
            std::string p, kind;
            std::size_t vars = 0, cls = 0;
            if (!(ss >> p >> kind >> vars >> cls) || p != "p" || kind != "cnf")
              throw ParseError (line_no, first + 1, "expected 'p cnf <vars> <clauses>'");
            declared_vars = vars;
            declared_clauses = cls;
            continue;
          }

        std::stringstream ss (line.substr (first));
        long long raw = 0;
        bool saw = false;
        while (ss >> raw)
          {
            saw = true;
            if (raw < std::numeric_limits<Lit>::min () || raw > std::numeric_limits<Lit>::max ())
              throw ParseError (line_no, first + 1, "literal exceeds int32 range");
            Lit lit = static_cast<Lit> (raw);
            if (lit == 0)
              {
                clauses.push_back (std::move (current));
                current.clear ();
              }
            else
              {
                if (declared_vars && static_cast<std::size_t> (literal_var (lit)) > *declared_vars)
                  throw ParseError (line_no, first + 1, "literal exceeds declared variable count");
                current.push_back (lit);
              }
          }
        if (!saw)
          throw ParseError (line_no, first + 1, "expected literal or comment");
      }

    if (!current.empty ())
      throw ParseError (line_no, 1, "unterminated clause; missing 0");
    if (declared_clauses && clauses.size () != *declared_clauses)
      throw ParseError (line_no, 1, "clause count does not match problem line");

    CNF cnf (std::move (clauses));
    if (declared_vars)
      cnf.set_declared_variable_count (*declared_vars);
    return cnf;
  }

private:
  static std::size_t first_non_ws (const std::string &line)
  {
    for (std::size_t i = 0; i < line.size (); ++i)
      if (!std::isspace (static_cast<unsigned char> (line[i])))
        return i;
    return std::string::npos;
  }

  std::istream &input_;
};

inline CNF parse_dimacs (std::istream &input) { return DimacsParser (input).parse (); }
inline CNF parse_dimacs_file (const std::string &path)
{
  std::ifstream file (path);
  if (!file)
    throw std::runtime_error ("failed to open DIMACS file: " + path);
  return parse_dimacs (file);
}

inline Lit x (Var v) { return make_literal (v, false); }
inline Lit nx (Var v) { return make_literal (v, true); }
inline Clause clause () { return {}; }
template <typename... Ts> Clause clause (Ts... lits) { return Clause{ static_cast<Lit> (lits)... }; }
template <typename... Clauses> CNF cnf_of (Clauses &&...clauses)
{
  CNF cnf;
  (cnf.add_clause (std::forward<Clauses> (clauses)), ...);
  return cnf;
}

class SymbolTable
{
public:
  Var intern (const std::string &name)
  {
    if (auto it = vars_.find (name); it != vars_.end ())
      return it->second;
    Var id = static_cast<Var> (vars_.size () + 1);
    vars_[name] = id;
    names_[id] = name;
    return id;
  }
  std::optional<Var> lookup (const std::string &name) const
  {
    if (auto it = vars_.find (name); it != vars_.end ())
      return it->second;
    return std::nullopt;
  }
  std::string name (Var v) const
  {
    if (auto it = names_.find (v); it != names_.end ())
      return it->second;
    return "x" + std::to_string (v);
  }

private:
  std::unordered_map<std::string, Var> vars_;
  std::unordered_map<Var, std::string> names_;
};

class CNFParser
{
public:
  explicit CNFParser (std::string input) : input_ (std::move (input)) {}

  CNF parse ()
  {
    CNF cnf;
    skip_ws ();
    if (eof ())
      return cnf;
    while (true)
      {
        cnf.add_clause (parse_clause ());
        skip_ws ();
        if (eof ())
          break;
        expect ('&');
        skip_ws ();
        if (eof ())
          fail ("expected clause after '&'");
      }
    return dimacs_to_cnf (cnf_to_dimacs (cnf));
  }

private:
  Clause parse_clause ()
  {
    expect ('(');
    skip_ws ();
    Clause out;
    if (peek () == ')')
      fail ("empty clause must be written in DIMACS, not CNF DSL");
    while (true)
      {
        out.push_back (parse_literal ());
        skip_ws ();
        if (peek () != '|')
          break;
        consume ();
        skip_ws ();
      }
    expect (')');
    return out;
  }

  Lit parse_literal ()
  {
    bool neg = false;
    if (peek () == '~' || peek () == '!')
      {
        neg = true;
        consume ();
      }
    if (!std::isalpha (static_cast<unsigned char> (peek ())) && peek () != '_')
      fail ("expected identifier");
    std::string ident;
    while (!eof () && (std::isalnum (static_cast<unsigned char> (peek ())) || peek () == '_'))
      ident.push_back (consume ());
    Var v = symbols_.intern (ident);
    return neg ? -v : v;
  }

  void skip_ws ()
  {
    while (!eof () && std::isspace (static_cast<unsigned char> (peek ())))
      consume ();
  }
  bool eof () const { return pos_ >= input_.size (); }
  char peek () const { return eof () ? '\0' : input_[pos_]; }
  char consume ()
  {
    char c = input_[pos_++];
    if (c == '\n')
      {
        ++line_;
        column_ = 1;
      }
    else
      ++column_;
    return c;
  }
  void expect (char c)
  {
    if (peek () != c)
      fail (std::string ("expected '") + c + "'");
    consume ();
  }
  [[noreturn]] void fail (const std::string &message) const { throw ParseError (line_, column_, message); }

  std::string input_;
  std::size_t pos_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  SymbolTable symbols_;
};

inline CNF parse_cnf (const std::string &text) { return CNFParser (text).parse (); }

struct ProblemDAG
{
  dsl::ASTNode root;
};

inline ProblemDAG cnf_to_dag (const CNF &cnf)
{
  std::vector<dsl::ASTNode> clauses;
  clauses.reserve (cnf.clause_count ());
  for (std::size_t i = 0; i < cnf.clauses ().size (); ++i)
    {
      std::vector<dsl::ASTNode> lits;
      for (Lit lit : cnf.clauses ()[i])
        lits.push_back (dsl::node<"literal"> (dsl::leaf<"var"> (literal_var (lit)),
                                               dsl::leaf<"sign"> (lit < 0 ? "neg" : "pos")));
      clauses.push_back (dsl::ASTNode{ "clause", std::move (lits) });
    }
  return ProblemDAG{ dsl::ASTNode{ "cnf", std::move (clauses) } };
}

inline std::string graphviz_escape (std::string_view s)
{
  std::string out;
  for (char c : s)
    {
      if (c == '"' || c == '\\')
        out.push_back ('\\');
      out.push_back (c);
    }
  return out;
}

inline std::string dag_to_dot (const ProblemDAG &dag)
{
  std::ostringstream dot;
  dot << "digraph SatieCNF {\n  rankdir=LR;\n  node [shape=box,fontname=monospace];\n";
  std::size_t next = 0;
  std::unordered_map<std::string, std::size_t> interned_literals;

  auto emit = [&] (auto &&self, const dsl::ASTNode &node) -> std::size_t {
    std::string key;
    if (node.tag () == "literal")
      key = node.dump ();
    if (!key.empty ())
      if (auto it = interned_literals.find (key); it != interned_literals.end ())
        return it->second;

    std::size_t id = next++;
    if (!key.empty ())
      interned_literals[key] = id;
    std::string label = std::string (node.tag ());
    if (node.is_leaf ())
      label += ":" + node.value ();
    dot << "  n" << id << " [label=\"" << graphviz_escape (label) << "\"];\n";
    for (const auto &child : node.children ())
      {
        std::size_t cid = self (self, child);
        dot << "  n" << id << " -> n" << cid << ";\n";
      }
    return id;
  };

  emit (emit, dag.root);
  dot << "}\n";
  return dot.str ();
}
inline std::string cnf_to_dot (const CNF &cnf) { return dag_to_dot (cnf_to_dag (cnf)); }

inline std::ostream &operator<< (std::ostream &os, const Clause &clause)
{
  os << '(';
  for (std::size_t i = 0; i < clause.size (); ++i)
    {
      if (i)
        os << " v ";
      if (clause[i] < 0)
        os << '~';
      os << 'x' << literal_var (clause[i]);
    }
  return os << ')';
}
inline std::ostream &operator<< (std::ostream &os, const CNF &cnf)
{
  for (std::size_t i = 0; i < cnf.clauses ().size (); ++i)
    {
      if (i)
        os << " ^ ";
      os << cnf.clauses ()[i];
    }
  return os;
}

} // namespace satie
