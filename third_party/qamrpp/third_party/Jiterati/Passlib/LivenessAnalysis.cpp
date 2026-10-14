/** @file Passlib/LivenessAnalysis.cpp
 *  @brief Backward liveness analysis.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace jiterati {

class LivenessAnalysis : public AnalysisPass, public FunctionPass {
public:
    std::string name() const override { return "liveness-analysis"; }

    void run(Function& fn) override {
        live_in_.clear();
        live_out_.clear();

        for (auto const& b : fn.blocks()) {
            live_in_[b.get()];
            live_out_[b.get()];
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto const& bup : fn.blocks()) {
                Block* b = bup.get();
                auto old_in = live_in_[b];
                auto old_out = live_out_[b];

                std::unordered_set<uint32_t> new_in = live_out_[b];
                for (auto it = b->instructions().rbegin(); it != b->instructions().rend(); ++it) {
                    Instruction* inst = *it;
                    if (!inst->type().is_void()) {
                        new_in.erase(inst->id());
                    }
                    for (auto const& op : inst->operands()) {
                        if (op.is_vreg()) new_in.insert(op.vreg_index());
                    }
                }

                std::unordered_set<uint32_t> new_out;
                for (auto* succ : b->successors()) {
                    auto const& sin = live_in_[succ];
                    new_out.insert(sin.begin(), sin.end());
                }

                if (new_in != old_in || new_out != old_out) {
                    live_in_[b].swap(new_in);
                    live_out_[b].swap(new_out);
                    changed = true;
                }
            }
        }
    }

    std::unordered_set<uint32_t> const& live_in(Block* b) const {
        return live_in_.at(b);
    }
    std::unordered_set<uint32_t> const& live_out(Block* b) const {
        return live_out_.at(b);
    }

private:
    std::unordered_map<Block*, std::unordered_set<uint32_t>> live_in_;
    std::unordered_map<Block*, std::unordered_set<uint32_t>> live_out_;
};

} // namespace jiterati
