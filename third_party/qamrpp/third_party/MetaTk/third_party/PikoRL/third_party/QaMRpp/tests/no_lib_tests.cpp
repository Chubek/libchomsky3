#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../QaMRpp.cpp"

using qamrpp::Context;
using qamrpp::Value;

static void expect_throw(const std::string& src) {
    Context ctx;
    bool threw = false;
    try {
        (void)ctx.run(src);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

int main() {
    int passed = 0;

    // 1
    {
        Context ctx;
        auto v = ctx.run("1");
        assert(v->type == Value::INT);
        assert(v->int_value == 1);
        ++passed;
    }

    // 2
    {
        Context ctx;
        auto v = ctx.run("1 + 2 * 3");
        assert(v->type == Value::FLOAT);
        assert(std::fabs(v->float_value - 7.0) < 1e-12);
        ++passed;
    }

    // 3
    {
        Context ctx;
        auto v = ctx.run("(1 + 2) * 3");
        assert(v->type == Value::FLOAT);
        assert(std::fabs(v->float_value - 9.0) < 1e-12);
        ++passed;
    }

    // 4
    {
        Context ctx;
        auto v = ctx.run("10 - 4 - 1");
        assert(v->type == Value::FLOAT);
        assert(std::fabs(v->float_value - 5.0) < 1e-12);
        ++passed;
    }

    // 5
    {
        Context ctx;
        auto v = ctx.run("8 / 2");
        assert(v->type == Value::FLOAT);
        assert(std::fabs(v->float_value - 4.0) < 1e-12);
        ++passed;
    }

    // 6
    {
        Context ctx;
        auto v = ctx.run("x = 10; x");
        assert(v->type == Value::INT);
        assert(v->int_value == 10);
        ++passed;
    }

    // 7
    {
        Context ctx;
        auto v = ctx.run("x = 10; y = x + 5; y");
        assert(v->type == Value::FLOAT);
        assert(std::fabs(v->float_value - 15.0) < 1e-12);
        ++passed;
    }

    // 8
    {
        Context ctx;
        auto v = ctx.run("\"a\" + \"b\"");
        assert(v->type == Value::STRING);
        assert(v->string_value == "ab");
        ++passed;
    }

    // 9
    {
        Context ctx;
        auto v = ctx.run("\"num:\" + 5");
        assert(v->type == Value::STRING);
        assert(v->string_value == "num:5");
        ++passed;
    }

    // 10
    {
        Context ctx;
        auto v = ctx.run("str(42)");
        assert(v->type == Value::STRING);
        assert(v->string_value == "42");
        ++passed;
    }

    // 11
    {
        Context ctx;
        auto v = ctx.run("str()");
        assert(v->type == Value::STRING);
        assert(v->string_value.empty());
        ++passed;
    }

    // 12
    {
        expect_throw("unknown");
        ++passed;
    }

    // 13
    {
        expect_throw("1 = 2");
        ++passed;
    }

    // 14
    {
        expect_throw("\"abc");
        ++passed;
    }

    // 15
    {
        expect_throw("1..2");
        ++passed;
    }

    std::cout << "Passed " << passed << " / 15 tests\n";
    return 0;
}
