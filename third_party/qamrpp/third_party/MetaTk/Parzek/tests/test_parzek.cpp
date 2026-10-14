#include "../Parzek.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("compile from string emits parser files") {
  std::filesystem::create_directories("/tmp/parzek-tests");
  const std::string grammar = R"(
@parser:name(Foo)
WS: [ \t\r\n]+ -> @IGNORE;
INTEGER: [0-9]+ when $0 != '0';
expr: INTEGER ("+" ~ INTEGER)*;
)";

  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  options.output_basename = "Foo";
  options.visitor_header = "FooVisitor.hpp";

  auto result = parzek::compile_grammar_string(grammar, options);
  REQUIRE(result.success);
  REQUIRE(std::filesystem::exists(result.parser_header_path));
  REQUIRE(std::filesystem::exists(result.parser_source_path));
  REQUIRE(result.visitor_header_path.has_value());
  REQUIRE(std::filesystem::exists(*result.visitor_header_path));
}

TEST_CASE("compile from file emits Foo-Parzek files") {
  std::filesystem::create_directories("/tmp/parzek-tests");
  const std::filesystem::path input = "/tmp/parzek-tests/Calc.pzg";
  std::ofstream out(input);
  out << R"(
@parser:name(Calc)
WS: [ \t\r\n]+ -> @IGNORE;
NUMBER: [0-9]+;
calc: NUMBER | NUMBER ~ "+" ~ NUMBER;
)";
  out.close();

  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  auto result = parzek::compile_grammar_file(input.string(), options);

  REQUIRE(result.success);
  REQUIRE(std::filesystem::exists("/tmp/parzek-tests/Calc-Parzek.hpp"));
  REQUIRE(std::filesystem::exists("/tmp/parzek-tests/Calc-Parzek.cpp"));
}

TEST_CASE("missing parser name reports diagnostic") {
  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  auto result = parzek::compile_grammar_string("FOO: \"bar\";", options);
  REQUIRE_FALSE(result.success);
  REQUIRE_FALSE(result.diagnostics.empty());
}

TEST_CASE("invalid syntax returns diagnostic failure") {
  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  auto result = parzek::compile_grammar_string(R"(
@parser:name(Bad)
TOK "x";
root: TOK;
)", options);
  REQUIRE_FALSE(result.success);
  REQUIRE_FALSE(result.diagnostics.empty());
}
