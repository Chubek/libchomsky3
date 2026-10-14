#ifndef QAMRPP_COMPILE_2_C_HPP
#define QAMRPP_COMPILE_2_C_HPP

#include <string>

#include "../QaMRpp.hpp"

namespace qamrpp {

class Compile2CPlugin : public Plugin {
public:
    const char* name() const { return "Compile2C"; }

    void install(Context& ctx) {
        ctx.register_native("compile_to_c", [](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            const std::string src = args.empty() ? std::string() : args[0]->to_string();
            std::string out = "#include <stdio.h>\nint main(void){\n";
            out += "  /* QaMRpp source length: " + std::to_string(src.size()) + " */\n";
            out += "  return 0;\n}\n";
            return std::make_shared<Value>(out);
        });
    }
};

} // namespace qamrpp

#endif
