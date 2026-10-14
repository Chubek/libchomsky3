/**
 * @file Parzek.hpp
 * @brief Public API for compiling Parzek grammars into C++ parser artifacts.
 *
 * Parzek accepts a compact grammar source, preprocesses macro/meta constructs,
 * parses lexical and syntactic rules, and emits parser headers, parser sources,
 * and optional visitor headers. The public interface is intentionally small:
 * callers pass a grammar string or file plus CompileOptions and receive explicit
 * output paths and diagnostics.
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace parzek {

/**
 * @brief Severity assigned to diagnostics emitted by grammar compilation.
 */
enum class DiagnosticSeverity {
  Info,
  Warning,
  Error
};

/**
 * @brief Source-positioned diagnostic produced during preprocessing, parsing, or generation.
 *
 * @var Diagnostic::severity
 * Classification used by clients to decide whether compilation may continue.
 * @var Diagnostic::message
 * Human-readable failure or warning text.
 * @var Diagnostic::file
 * Logical source name or filesystem path associated with the diagnostic.
 * @var Diagnostic::line
 * One-based source line.
 * @var Diagnostic::column
 * One-based source column.
 */
struct Diagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::Error};
  std::string message;
  std::string file;
  std::size_t line{1};
  std::size_t column{1};
};

/**
 * @brief Host-controlled output and naming policy for grammar compilation.
 *
 * The compiler writes generated files under output_directory. output_basename
 * overrides the parser stem inferred from the grammar. visitor_header requests
 * visitor generation. source_name is used in diagnostics for in-memory inputs.
 */
struct CompileOptions {
  std::string output_directory{"."};
  std::optional<std::string> output_basename;
  std::optional<std::string> visitor_header;
  std::string source_name{"<memory>"};
};

/**
 * @brief Result object returned by every public compilation entry point.
 *
 * success is true only when required parser artifacts were emitted. Path fields
 * name generated files when available. diagnostics contains all recoverable
 * errors and warnings observed during the run.
 */
struct CompileResult {
  bool success{false};
  std::string parser_header_path;
  std::string parser_source_path;
  std::optional<std::string> visitor_header_path;
  std::vector<Diagnostic> diagnostics;
};

/**
 * @brief Compile an in-memory Parzek grammar into generated C++ artifacts.
 * @param grammar_source Grammar text to preprocess, parse, validate, and emit.
 * @param options Output paths, source name, and optional visitor settings.
 * @return CompileResult with artifact paths and diagnostics.
 */
CompileResult compile_grammar_string(std::string_view grammar_source,
                                     const CompileOptions& options);

/**
 * @brief Compile a grammar file from disk.
 * @param grammar_path Path to the grammar source file.
 * @param options Output paths and optional generation settings.
 * @return CompileResult with I/O, parse, validation, and emission diagnostics.
 */
CompileResult compile_grammar_file(const std::string& grammar_path,
                                   const CompileOptions& options);

/**
 * @brief Execute the Parzek command-line interface.
 * @param argc Process argument count.
 * @param argv Process argument vector.
 * @return Zero on successful generation, non-zero on parse, I/O, or usage failure.
 */
int run_cli(int argc, char** argv);

}  // namespace parzek
