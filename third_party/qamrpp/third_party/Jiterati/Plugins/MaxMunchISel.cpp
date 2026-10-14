/** @file Plugins/MaxMunchISel.cpp
 *  @brief Maximal-munch instruction selector stub.
 */
#include "../include/Jiterati.hpp"
#include "../include/Jiterati-Plugin.hpp"

#include <vector>

namespace jiterati {

class MaxMunchISel {
public:
    struct Match { Opcode op; int cost; };

    void add_match(Opcode op, int cost) {
        matches_.push_back({ op, cost });
    }
    std::size_t match_count() const { return matches_.size(); }
private:
    std::vector<Match> matches_;
};

std::size_t max_munch_default_match_count() {
    PluginRegistry::instance().register_plugin("MaxMunchISel", { 0, 1, 0 });
    MaxMunchISel isel;
    isel.add_match(Opcode::Mul, 3);
    isel.add_match(Opcode::Add, 2);
    isel.add_match(Opcode::Load, 1);
    return isel.match_count();
}

} // namespace jiterati
