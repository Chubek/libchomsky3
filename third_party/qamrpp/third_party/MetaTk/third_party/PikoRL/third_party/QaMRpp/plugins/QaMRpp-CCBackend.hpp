#ifndef QAMRPP_CC_BACKEND_HPP
#define QAMRPP_CC_BACKEND_HPP

#include <string>

#include "../QaMRpp.hpp"

namespace qamrpp {

class CCBackendPlugin : public Plugin {
public:
    const char* name() const { return "CCBackend"; }

    void install(Context& ctx) {
        ctx.register_native("compile_c", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            std::string source = args.empty() ? "" : args[0]->to_string();
            std::string c_code = "#include <stdio.h>\nint main(void){puts(\"" + source + "\");return 0;}\n";
            return std::make_shared<Value>(c_code);
        });
    }
};

} // namespace qamrpp

#endif
