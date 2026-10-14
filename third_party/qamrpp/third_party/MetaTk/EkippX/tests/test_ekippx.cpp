#include "EkippX.hpp"
#include "EkippX-Batteries.hpp"
#include "EkippX-Plugin.hpp"
#include "plugins/FilesystemPlugin.hpp"
#include "plugins/EnvironmentPlugin.hpp"
#include "plugins/DateTimePlugin.hpp"
#include "plugins/RandomPlugin.hpp"
#include "plugins/TextPlugin.hpp"

#include <catch_amalgamated.hpp>

TEST_CASE("basic expansion works") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE(ctx.expand_text("@emitln(&upper(hello))") == "HELLO\n");
}

TEST_CASE("macro definitions expand") {
  auto ctx = ekippx::batteries::batteries_context();
  const auto out = ctx.expand_text("@define(PAIR, `$1::$2`) @emitln(&PAIR(a, b))");
  REQUIRE(out.find("a::b") != std::string::npos);
}

TEST_CASE("conditionals choose the correct branch") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE(ctx.expand_text("@if(`IfEq?(x, x)`, `ok`, `no`)") == "ok");
}

TEST_CASE("token dump recognizes directives and functions") {
  auto ctx = ekippx::batteries::batteries_context();
  const auto parsed = ctx.lex("@emitln(&echo(x))");
  REQUIRE(parsed.success);
  REQUIRE_FALSE(parsed.tokens.empty());
}

TEST_CASE("plugin manager installs static plugins") {
  auto ctx = ekippx::batteries::batteries_context();
  ekippx::plugin::PluginManager manager(ctx);
  manager.register_static(ekippx::plugins::make_text_plugin());
  REQUIRE(ctx.activate_plugin("text"));
  REQUIRE(ctx.expand_text("@emitln(&reverseText(stressed))") == "desserts\n");
}

TEST_CASE("filesystem plugin functions work") {
  auto ctx = ekippx::batteries::batteries_context();
  ekippx::plugin::PluginManager manager(ctx);
  manager.register_static(ekippx::plugins::make_filesystem_plugin());
  REQUIRE(ctx.activate_plugin("filesystem"));
  REQUIRE(ctx.expand_text("@emitln(&basename(/tmp/demo.txt))") == "demo.txt\n");
}

TEST_CASE("syntax file exists and is readable") {
  std::ifstream syntax("EkippX.syntax");
  if (!syntax.good()) {
    syntax = std::ifstream("/home/chubakpdp11/lambda/MetaTk/EkippX/EkippX.syntax");
  }
  REQUIRE(syntax.good());
  std::string text((std::istreambuf_iterator<char>(syntax)), std::istreambuf_iterator<char>());
  REQUIRE(text.find("directive_name") != std::string::npos);
}

TEST_CASE("guards reject malformed identifiers and numeric inputs") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE_THROWS_AS(ctx.expand_text("@define(bad/name, x)"), ekippx::Error);
  REQUIRE_THROWS_AS(ctx.expand_text("@emitln(&add(1, nope))"), ekippx::Error);
  REQUIRE_THROWS_AS(ctx.expand_text("@counter(../x, 1)"), ekippx::Error);
}

TEST_CASE("include sandbox rejects traversal") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE_THROWS_AS(ctx.expand_text("@include(../secret.txt)"), ekippx::IncludeError);
}

TEST_CASE("trace captures nested expansion success and failures") {
  auto ctx = ekippx::batteries::batteries_context();
  ctx.config().runtime.trace_enabled = true;
  REQUIRE(ctx.expand_text("@emitln(&upper(ok))") == "OK\n");
  REQUIRE(ctx.trace().size() >= 4);
  REQUIRE(ctx.trace().front().callee == "upper");
  REQUIRE(ctx.trace().back().kind == ekippx::TraceEventKind::exit);

  const auto failed = ctx.expand(ekippx::ExpandRequest{"<test>", "@emitln(&add(nope, 1))", std::nullopt});
  REQUIRE_FALSE(failed.success);
  REQUIRE(std::any_of(ctx.trace().begin(), ctx.trace().end(), [](const ekippx::TraceEvent& event) {
    return event.kind == ekippx::TraceEventKind::error && event.callee == "add";
  }));
}

TEST_CASE("symbol table dump contains batteries and plugins without environment values") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE(ctx.activate_plugin("text"));
  const auto dump = ctx.dump_symbols();
  REQUIRE(std::any_of(dump.symbols.begin(), dump.symbols.end(), [](const ekippx::SymbolRecord& record) {
    return record.name == "emitln" && record.kind == "directive";
  }));
  REQUIRE(std::any_of(dump.symbols.begin(), dump.symbols.end(), [](const ekippx::SymbolRecord& record) {
    return record.name == "reverseText" && record.kind == "function";
  }));
  const auto json = serdetk::builtins::json().dump_string(ctx.symbol_document());
  REQUIRE(json.find("emitln") != std::string::npos);
  REQUIRE(json.find("HOSTNAME") == std::string::npos);
}

TEST_CASE("filesystem and environment plugins guard unsafe inputs") {
  auto ctx = ekippx::batteries::batteries_context();
  REQUIRE(ctx.activate_plugin("filesystem"));
  REQUIRE(ctx.activate_plugin("environment"));
  REQUIRE_THROWS_AS(ctx.expand_text("@emitln(&basename(../x))"), ekippx::FunctionError);
  REQUIRE_THROWS_AS(ctx.expand_text("@emitln($ENV{BAD-NAME})"), ekippx::ExpanderError);
}
