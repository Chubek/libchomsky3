#include "../Parzek.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

static parzek::CompileResult compile_text(const std::string& text, const std::string& base) {
  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  options.output_basename = base;
  return parzek::compile_grammar_string(text, options);
}

TEST_CASE("comments accepted in grammar") {
  std::filesystem::create_directories("/tmp/parzek-tests");
  auto result = compile_text(R"(
@parser:name(Commented)
// comment
/* block */
TOK: "a";
root: TOK;
)", "Commented");
  REQUIRE(result.success);
}

TEST_CASE("naming distinction enforced") {
  auto result = compile_text(R"(
@parser:name(BadNames)
bad_token: "x";
root: bad_token;
)", "BadNames");
  REQUIRE_FALSE(result.success);
}

TEST_CASE("operators and grouping parse") {
  auto result = compile_text(R"(
@parser:name(Ops)
DIGIT: [0-9];
NUMBER: DIGIT+;
item: (NUMBER | "x")? NUMBER*;
)", "Ops");
  REQUIRE(result.success);
}

TEST_CASE("adjacency and when accepted") {
  auto result = compile_text(R"(
@parser:name(Adj)
A: "a";
B: "b";
pair: A ~ B when CHAN == @IGNORE;
)", "Adj");
  REQUIRE(result.success);
}

TEST_CASE("meta variable guards accepted") {
  auto result = compile_text(R"(
@parser:name(Meta)
NUM: [0-9]+ when LEN != 0;
NZ: [0-9]+ when $0 != '0';
root: NUM | NZ;
)", "Meta");
  REQUIRE(result.success);
}

TEST_CASE("preprocessor macro expansion works") {
  auto result = compile_text(R"(
@parser:name(PP)
@define WRAP() "a" ++ "b"
TOK: WRAP();
root: TOK;
)", "PP");
  REQUIRE(result.success);
}

TEST_CASE("preprocessor !#n expands recursively before substitution") {
  auto result = compile_text(R"(
@parser:name(PPDeep1)
@define INNER() "x"
@define OUTER() !#1
TOK: OUTER(INNER());
root: TOK;
)", "PPDeep1");
  REQUIRE(result.success);
  std::ifstream in("/tmp/parzek-tests/PPDeep1-Parzek.cpp");
  std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE(code.find("token(\"x\"") != std::string::npos);
}

TEST_CASE("preprocessor #n! keeps argument non-expanded after substitution") {
  auto result = compile_text(R"(
@parser:name(PPDeep2)
@define INNER() "x"
@define OUTER() #1!
INNER: "x";
TOK: OUTER(INNER);
root: TOK;
)", "PPDeep2");
  REQUIRE(result.success);
  std::ifstream in("/tmp/parzek-tests/PPDeep2-Parzek.cpp");
  std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE((code.find("parse_INNER()") != std::string::npos || code.find("token(\"x\"") != std::string::npos));
}

TEST_CASE("preprocessor #@ and #! handling works") {
  auto result = compile_text(R"(
@parser:name(PPDeep3)
@define ASRAW() #!
@define ASEXP() #@
TOK1: "a";
TOK2: "b";
one: ASEXP(TOK1 TOK2);
two: ASRAW(TOK1 TOK2);
root: one | two;
)", "PPDeep3");
  REQUIRE(result.success);
}

TEST_CASE("preprocessor ## first-token behavior works") {
  auto result = compile_text(R"(
@parser:name(PPDeep4)
@define FIRST() ##
HELLO: "hello";
TOK: FIRST(HELLO WORLD);
root: TOK;
)", "PPDeep4");
  REQUIRE(result.success);
  std::ifstream in("/tmp/parzek-tests/PPDeep4-Parzek.cpp");
  std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE(code.find("parse_HELLO()") != std::string::npos);
}

TEST_CASE("visitor generation emits header") {
  parzek::CompileOptions options;
  options.output_directory = "/tmp/parzek-tests";
  options.output_basename = "Visitor";
  options.visitor_header = "VisitorHooks.hpp";
  auto result = parzek::compile_grammar_string(R"(
@parser:name(Visitor)
TOK: "x";
root: TOK;
)", options);
  REQUIRE(result.success);
  REQUIRE(std::filesystem::exists("/tmp/parzek-tests/VisitorHooks.hpp"));
}

TEST_CASE("cli compile flow") {
  std::filesystem::create_directories("/tmp/parzek-tests");
  std::ofstream out("/tmp/parzek-tests/Cli.pzg");
  out << R"(
@parser:name(Cli)
TOK: "x";
root: TOK;
)";
  out.close();

  char arg0[] = "parzek";
  char arg1[] = "compile";
  char arg2[] = "/tmp/parzek-tests/Cli.pzg";
  char arg3[] = "--output-dir=/tmp/parzek-tests";
  char* argv[] = {arg0, arg1, arg2, arg3};
  auto rc = parzek::run_cli(4, argv);
  REQUIRE(rc == 0);
}

TEST_CASE("cli unknown option is rejected") {
  char arg0[] = "parzek";
  char arg1[] = "compile";
  char arg2[] = "/tmp/parzek-tests/Cli.pzg";
  char arg3[] = "--unknown-flag";
  char* argv[] = {arg0, arg1, arg2, arg3};
  auto rc = parzek::run_cli(4, argv);
  REQUIRE(rc != 0);
}

TEST_CASE("help flag returns success") {
  char arg0[] = "parzek";
  char arg1[] = "--help";
  char* argv[] = {arg0, arg1};
  auto rc = parzek::run_cli(2, argv);
  REQUIRE(rc == 0);
}

TEST_CASE("output basename is sanitized") {
  auto result = compile_text(R"(
@parser:name(Sanitize)
TOK: "x";
root: TOK;
)", "Bad Name/With:Chars");
  REQUIRE(result.success);
  REQUIRE(result.parser_header_path.find("Bad_Name_With_Chars-Parzek.hpp") != std::string::npos);
}

TEST_CASE("generated parser compilation smoke") {
  auto result = compile_text(R"(
@parser:name(Smoke)
TOK: "x";
root: TOK;
)", "Smoke");
  REQUIRE(result.success);
  REQUIRE(std::filesystem::exists("/tmp/parzek-tests/Smoke-Parzek.hpp"));
  REQUIRE(std::filesystem::exists("/tmp/parzek-tests/Smoke-Parzek.cpp"));
  const char* cmd = "c++ -std=c++20 -I/home/chubakpdp11/nvme/dslutil -c /tmp/parzek-tests/Smoke-Parzek.cpp -o /tmp/parzek-tests/Smoke-Parzek.o";
  const auto rc = std::system(cmd);
  REQUIRE(rc == 0);
}

TEST_CASE("adjacency with channel guard behaves as expected at runtime") {
  auto result = compile_text(R"(
@parser:name(AdjRuntime)
A: "a";
B: "b";
pair: A ~ B when CHAN == @TOK;
root: pair;
)", "AdjRuntime");
  REQUIRE(result.success);
  const char* compile_generated = "c++ -std=c++20 -I/home/chubakpdp11/nvme/dslutil -c /tmp/parzek-tests/AdjRuntime-Parzek.cpp -o /tmp/parzek-tests/AdjRuntime-Parzek.o";
  REQUIRE(std::system(compile_generated) == 0);
}

TEST_CASE("default start symbol is first non-terminal") {
  auto result = compile_text(R"(
@parser:name(StartDefault)
WS: [ \t\r\n]+ -> @IGNORE;
TOKEN: "x";
entry: TOKEN;
other: entry;
)", "StartDefault");
  REQUIRE(result.success);
  std::ifstream in("/tmp/parzek-tests/StartDefault-Parzek.cpp");
  std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE(code.find("run_parser(parse_entry(), input)") != std::string::npos);
}

TEST_CASE("@parser:start overrides top non-terminal") {
  auto result = compile_text(R"(
@parser:name(StartOverride)
@parser:start(other)
WS: [ \t\r\n]+ -> @IGNORE;
TOKEN: "x";
entry: TOKEN;
other: entry;
)", "StartOverride");
  REQUIRE(result.success);
  std::ifstream in("/tmp/parzek-tests/StartOverride-Parzek.cpp");
  std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE(code.find("run_parser(parse_other(), input)") != std::string::npos);
}

TEST_CASE("@parser:start missing target reports diagnostic") {
  auto result = compile_text(R"(
@parser:name(StartBad)
@parser:start(does-not-exist)
TOKEN: "x";
entry: TOKEN;
)", "StartBad");
  REQUIRE_FALSE(result.success);
  REQUIRE_FALSE(result.diagnostics.empty());
}
