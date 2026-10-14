/** @file Plugins/BURSISel.cpp
 *  @brief Bottom-up rewrite system instruction selector stub.
 */
#include "../include/Jiterati.hpp"
#include "../include/Jiterati-Plugin.hpp"

#include <memory>
#include <vector>

namespace jiterati {

class BURSPattern {
public:
    virtual ~BURSPattern() = default;
    virtual bool match(Instruction const& inst) const = 0;
};

class BURSISel {
public:
    void register_pattern(std::unique_ptr<BURSPattern> p) {
        patterns_.push_back(std::move(p));
    }
    std::size_t pattern_count() const { return patterns_.size(); }
private:
    std::vector<std::unique_ptr<BURSPattern>> patterns_;
};

class OpcodePattern final : public BURSPattern {
public:
    explicit OpcodePattern(Opcode expected) : expected_(expected) {}
    bool match(Instruction const& inst) const override { return inst.opcode() == expected_; }
private:
    Opcode expected_;
};

std::size_t burs_default_pattern_count() {
    PluginRegistry::instance().register_plugin("BURSISel", { 0, 1, 0 });
    BURSISel isel;
    isel.register_pattern(std::make_unique<OpcodePattern>(Opcode::Add));
    isel.register_pattern(std::make_unique<OpcodePattern>(Opcode::Mul));
    isel.register_pattern(std::make_unique<OpcodePattern>(Opcode::Load));
    return isel.pattern_count();
}

} // namespace jiterati
