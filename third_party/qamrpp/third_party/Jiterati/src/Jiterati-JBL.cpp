/** @file Jiterati-JBL.cpp
 *  @brief Minimal JBL printer/parser over the core Jiterati IR.
 */
#include "../include/Jiterati.hpp"

#include <cctype>
#include <memory>
#include <sstream>
#include <string_view>

namespace jiterati {

namespace {

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

bool parse_type(std::string const& text, Type& out) {
    if (text == "void") out = Type::void_ty();
    else if (text == "i1") out = Type::i1();
    else if (text == "i8") out = Type::i8();
    else if (text == "i16") out = Type::i16();
    else if (text == "i32") out = Type::i32();
    else if (text == "i64") out = Type::i64();
    else if (text == "f32") out = Type::f32();
    else if (text == "f64") out = Type::f64();
    else if (text == "ptr") out = Type::ptr();
    else return false;
    return true;
}

} // namespace

std::string print_jbl(Module const& module) {
    return module.to_string();
}

std::unique_ptr<Module> parse_jbl(std::string_view source, std::string* error) {
    std::istringstream input{std::string(source)};
    std::string line;
    if (!std::getline(input, line)) {
        if (error) *error = "empty JBL source";
        return nullptr;
    }
    line = trim(line);
    if (line.rfind("module ", 0) != 0) {
        if (error) *error = "expected module header";
        return nullptr;
    }
    std::string module_name = line.substr(7);
    if (module_name.size() >= 2 && module_name.front() == '"' && module_name.back() == '"') {
        module_name = module_name.substr(1, module_name.size() - 2);
    }
    auto module = std::make_unique<Module>(module_name);

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("const ", 0) == 0 || line.back() == ':' || line == "}") continue;
        if (line.rfind("func ", 0) != 0) continue;

        auto at = line.find('@');
        auto lparen = line.find('(', at == std::string::npos ? 0 : at);
        auto rparen = line.find(')', lparen == std::string::npos ? 0 : lparen);
        if (at == std::string::npos || lparen == std::string::npos || rparen == std::string::npos) {
            if (error) *error = "malformed function signature";
            return nullptr;
        }

        Type return_type;
        if (!parse_type(trim(std::string_view(line).substr(5, at - 5)), return_type)) {
            if (error) *error = "unknown return type";
            return nullptr;
        }

        std::string function_name = line.substr(at + 1, lparen - at - 1);
        std::vector<Type> params;
        std::string param_blob = line.substr(lparen + 1, rparen - lparen - 1);
        std::istringstream param_stream(param_blob);
        std::string param;
        while (std::getline(param_stream, param, ',')) {
            param = trim(param);
            if (param.empty()) continue;
            auto space = param.find(' ');
            std::string type_text = space == std::string::npos ? param : param.substr(0, space);
            Type param_type;
            if (!parse_type(type_text, param_type)) {
                if (error) *error = "unknown parameter type";
                return nullptr;
            }
            params.push_back(param_type);
        }

        Function* function = module->create_function(function_name, return_type, params);
        Block* entry = function->create_block("entry");
        entry->ret_void();
    }

    return module;
}


} // namespace jiterati
