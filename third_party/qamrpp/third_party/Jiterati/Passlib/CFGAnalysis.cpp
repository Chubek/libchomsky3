/** @file Passlib/CFGAnalysis.cpp
 *  @brief Control-flow graph analysis pass.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <vector>
#include <unordered_set>
#include <queue>

namespace jiterati {

class CFGAnalysis : public AnalysisPass, public FunctionPass {
public:
    std::string name() const override { return "cfg-analysis"; }

    void run(Function& fn) override {
        // Successor/predecessor edges are maintained by the block builders.
        // This pass validates and computes a post-order traversal.
        postorder_.clear();
        visited_.clear();
        if (fn.entry_block()) dfs(fn.entry_block());
    }

    std::vector<Block*> const& postorder() const { return postorder_; }

private:
    std::unordered_set<Block*> visited_;
    std::vector<Block*> postorder_;

    void dfs(Block* b) {
        if (!b || visited_.count(b)) return;
        visited_.insert(b);
        for (auto* succ : b->successors()) dfs(succ);
        postorder_.push_back(b);
    }
};

} // namespace jiterati
