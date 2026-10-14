/** @file Jiterati-Pass.hpp
 *  @brief Pass infrastructure (declarations only for now).
 */
#ifndef JITERATI_PASS_HPP_INCLUDED
#define JITERATI_PASS_HPP_INCLUDED

#include "Jiterati.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace jiterati {

class Pass {
public:
    virtual ~Pass() = default;
    virtual std::string name() const = 0;
};

class FunctionPass : public Pass {
public:
    virtual void run(Function& fn) = 0;
};

class ModulePass : public Pass {
public:
    virtual void run(Module& m) = 0;
};

class AnalysisPass : public Pass {
public:
    virtual bool is_analysis() const final { return true; }
};

class TransformPass : public Pass {
public:
    virtual bool is_transform() const final { return true; }
};

class PassManager {
public:
    void add(std::unique_ptr<Pass> pass);
    void run(Module& m);
private:
    std::vector<std::unique_ptr<Pass>> passes_;
};

class AnalysisManager {
public:
    // Placeholder for analysis result caching.
};

PassManager parse_jpl_pipeline(std::string_view source);

inline void PassManager::add(std::unique_ptr<Pass> pass) {
    passes_.push_back(std::move(pass));
}

inline void PassManager::run(Module& m) {
    for (auto& p : passes_) {
        if (auto* mp = dynamic_cast<ModulePass*>(p.get())) {
            mp->run(m);
        } else if (auto* fp = dynamic_cast<FunctionPass*>(p.get())) {
            for (auto& fn : m.functions()) {
                fp->run(*fn);
            }
        }
    }
}

} // namespace jiterati

#endif // JITERATI_PASS_HPP_INCLUDED
