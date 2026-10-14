#include "regexppc.h"
#include <QaMRpp.hpp>
#include <stdexcept>

namespace regexppc {
void run_lua(const std::string& source, spec& cfg) {
    qamrpp::Context context;
    context.globals["define"] = std::make_shared<qamrpp::Value>(qamrpp::NativeFn(
        [&cfg](qamrpp::Context&, std::vector<qamrpp::ValuePtr>& args) {
            if (args.size() != 2 || args[0]->type != qamrpp::Value::STRING || args[1]->type != qamrpp::Value::STRING)
                throw std::runtime_error("define(name, regex) requires two strings");
            cfg.definitions[args[0]->string_value] = args[1]->string_value;
            return std::make_shared<qamrpp::Value>();
        }));
    context.run(source);
}
}
