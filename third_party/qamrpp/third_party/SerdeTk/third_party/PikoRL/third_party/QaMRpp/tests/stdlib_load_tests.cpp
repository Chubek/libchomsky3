#include <cassert>
#include <string>

#include "../QaMRpp.hpp"
#include "../stdlib/core.cpp"
#include "../stdlib/string.cpp"
#include "../stdlib/table.cpp"
#include "../stdlib/math.cpp"
#include "../stdlib/io.cpp"
#include "../stdlib/os.cpp"
#include "../stdlib/debug.cpp"
#include "../stdlib/coroutine.cpp"
#include "../stdlib/package.cpp"
#include "../stdlib/utf8.cpp"

int main() {
    qamrpp::Context ctx;
    ctx.load_standard_library(qamrpp::StdLib::CORE | qamrpp::StdLib::MATH | qamrpp::StdLib::UTF8);

    auto t = ctx.run("type(1)");
    assert(t->type == qamrpp::Value::STRING);
    assert(t->string_value == "int");

    auto a = ctx.run("math_abs(0-3)");
    assert(a->type == qamrpp::Value::FLOAT);
    assert(a->float_value == 3.0);

    auto u = ctx.run("utf8_len(\"abc\")");
    assert(u->type == qamrpp::Value::INT);
    assert(u->int_value == 3);

    return 0;
}
