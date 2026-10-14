#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include "QaMRpp.cpp"

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}

TEST_CASE("arithmetic precedence") {
    qamrpp::Context ctx;
    auto result = ctx.run("1 + 2 * 3");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 7.0);
}

TEST_CASE("assignment and lookup") {
    qamrpp::Context ctx;
    ctx.run("x = 10");
    auto result = ctx.run("x");
    REQUIRE(result->type == qamrpp::Value::INT);
    REQUIRE(result->int_value == 10);
}

TEST_CASE("unterminated string throws") {
    qamrpp::Context ctx;
    REQUIRE_THROWS(ctx.run("\"abc"));
}

TEST_CASE("C plugin value api") {
    qamrpp::Context ctx;
    auto* v = qamrpp_value_string(reinterpret_cast<qamrpp_context*>(&ctx), "ok", 2);
    REQUIRE(v != nullptr);
    size_t len = 0;
    auto* s = qamrpp_value_as_string(v, &len);
    REQUIRE(s != nullptr);
    REQUIRE(len == 2);
    REQUIRE(std::string(s, len) == "ok");
    delete reinterpret_cast<qamrpp::Value*>(v);
}
