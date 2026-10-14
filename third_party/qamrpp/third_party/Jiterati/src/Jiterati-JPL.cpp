/** @file Jiterati-JPL.cpp
 *  @brief Minimal JPL pipeline parser.
 */
#include "../include/Jiterati-Pass.hpp"

#include <cctype>
#include <sstream>
#include <string_view>

namespace jiterati {

namespace {

std::vector<std::string> split_pipeline(std::string_view source) {
    std::vector<std::string> names;
    std::string current;
    for (char c : source) {
        if (c == ';' || std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                names.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) names.push_back(current);
    return names;
}

class NamedNoOpPass final : public ModulePass {
public:
    explicit NamedNoOpPass(std::string pass_name) : pass_name_(std::move(pass_name)) {}
    std::string name() const override { return pass_name_; }
    void run(Module&) override {}
private:
    std::string pass_name_;
};

} // namespace

PassManager parse_jpl_pipeline(std::string_view source) {
    PassManager manager;
    for (std::string const& name : split_pipeline(source)) {
        manager.add(std::make_unique<NamedNoOpPass>(name));
    }
    return manager;
}

} // namespace jiterati
