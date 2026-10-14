#include <cassert>
#include <fstream>
#include <string>

#include "../QaMRpp.hpp"

int main() {
    {
        qamrpp::Context ctx;
        qamrpp::Linker linker;
        linker.add_source("unit1.lua", "x = 2");
        linker.add_source("unit2.lua", "x + 3");
        qamrpp::ValuePtr result = linker.link(ctx);
        assert(result->type == qamrpp::Value::FLOAT);
        assert(result->float_value == 5.0);
    }

    {
        const std::string path = "tests/tmp_link.lua";
        {
            std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
            out << "y = 7; y";
        }
        qamrpp::Context ctx;
        assert(ctx.linker.add_file(path));
        qamrpp::ValuePtr result = ctx.linker.link(ctx);
        assert(result->type == qamrpp::Value::INT);
        assert(result->int_value == 7);
    }

    {
        qamrpp::Context ctx;
        qamrpp::Linker linker;
        linker.add_source("bad.lua", "unknown_fn(1)");
        bool failed = false;
        try {
            (void)linker.link(ctx);
        } catch (const std::runtime_error& e) {
            failed = std::string(e.what()).find("bad.lua") != std::string::npos;
        }
        assert(failed);
    }

    return 0;
}
