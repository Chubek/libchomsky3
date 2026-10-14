/** @file Plugins/ListISched.cpp
 *  @brief List scheduling plugin stub.
 */
#include "../include/Jiterati.hpp"
#include "../include/Jiterati-Plugin.hpp"

#include <algorithm>
#include <vector>

namespace jiterati {

class ListScheduler {
public:
    void schedule(Function& fn) {
        for (auto& block_up : fn.blocks()) {
            auto& instructions = block_up->instructions();
            std::stable_sort(instructions.begin(), instructions.end(), [](Instruction* lhs, Instruction* rhs) {
                if (lhs->is_terminator() != rhs->is_terminator()) return !lhs->is_terminator();
                return lhs->id() < rhs->id();
            });
        }
    }
    std::size_t block_count(Function& fn) const { return fn.blocks().size(); }
};

std::size_t schedule_blocks(Function& fn) {
    PluginRegistry::instance().register_plugin("ListISched", { 0, 1, 0 });
    ListScheduler scheduler;
    scheduler.schedule(fn);
    return scheduler.block_count(fn);
}

} // namespace jiterati
