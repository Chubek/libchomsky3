/** @file Passlib/DominatorAnalysis.cpp
 *  @brief Simple dominator tree analysis (Cooper-Harvey-Kennedy iterative).
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace jiterati {

class DominatorAnalysis : public AnalysisPass, public FunctionPass {
public:
    std::string name() const override { return "dominator-analysis"; }

    void run(Function& fn) override {
        doms_.clear();
        auto* entry = fn.entry_block();
        if (!entry) return;

        std::vector<Block*> blocks;
        for (auto const& b : fn.blocks()) blocks.push_back(b.get());

        for (auto* b : blocks) {
            doms_[b] = std::unordered_set<Block*>(blocks.begin(), blocks.end());
        }
        doms_[entry].clear();
        doms_[entry].insert(entry);

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto* b : blocks) {
                if (b == entry) continue;
                std::unordered_set<Block*> new_set;
                bool first = true;
                for (auto* p : b->predecessors()) {
                    if (first) {
                        new_set = doms_[p];
                        first = false;
                    } else {
                        std::unordered_set<Block*> intersect;
                        for (auto* x : new_set) {
                            if (doms_[p].count(x)) intersect.insert(x);
                        }
                        new_set.swap(intersect);
                    }
                }
                new_set.insert(b);
                if (new_set != doms_[b]) {
                    doms_[b].swap(new_set);
                    changed = true;
                }
            }
        }
    }

    bool dominates(Block* dominator, Block* dominated) const {
        auto it = doms_.find(dominated);
        if (it == doms_.end()) return false;
        return it->second.count(dominator) != 0;
    }

private:
    std::unordered_map<Block*, std::unordered_set<Block*>> doms_;
};

} // namespace jiterati
