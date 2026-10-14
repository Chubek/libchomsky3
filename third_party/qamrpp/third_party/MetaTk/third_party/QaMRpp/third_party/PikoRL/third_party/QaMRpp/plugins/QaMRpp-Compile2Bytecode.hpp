#ifndef QAMRPP_COMPILE_2_BYTECODE_HPP
#define QAMRPP_COMPILE_2_BYTECODE_HPP

#include <string>

#include "../QaMRpp.hpp"

namespace qamrpp {

class Compile2BytecodePlugin : public Plugin {
public:
    const char* name() const { return "Compile2Bytecode"; }

    void install(Context& ctx) {
        ctx.register_native("compile_to_bytecode", [](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            const std::string src = args.empty() ? std::string() : args[0]->to_string();
            std::string bc = "QBC0:" + std::to_string(src.size()) + ":" + src;
            return std::make_shared<Value>(bc);
        });
    }
};

} // namespace qamrpp

#endif
