#ifndef JITERATI_BE_AARCH64_AARCH64_HPP_INCLUDED
#define JITERATI_BE_AARCH64_AARCH64_HPP_INCLUDED

#include "../../IR/IR.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace jiterati::be::aarch64 {

enum class Reg {
    X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15,
    X16, X17, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, FP, LR, SP,
    V0, V1, V2, V3, V4, V5, V6, V7,
};

enum class RegClass { Gpr, Fpr };

struct ABI {
    std::vector<Reg> integer_args;
    std::vector<Reg> float_args;
    Reg integer_return = Reg::X0;
    Reg float_return = Reg::V0;
    std::vector<Reg> caller_saved;
    std::vector<Reg> callee_saved;
    std::size_t stack_alignment = 16;
};

struct MachineInst { std::string opcode; std::vector<std::string> operands; std::string comment; };
struct MachineBlock { std::string label; std::vector<MachineInst> instructions; };
struct MachineFunction {
    std::string name;
    std::vector<MachineBlock> blocks;
    std::map<std::string, Reg> vreg_assignment;
    std::size_t stack_size = 0;
};

char const* reg_name(Reg reg);
ABI aapcs64_abi();
RegClass reg_class(ir::Type type);
std::vector<Reg> allocatable_gprs();
std::vector<Reg> allocatable_fprs();
MachineFunction select(ir::TACFunction const& function);
void allocate_registers(MachineFunction& function);
void rewrite(MachineFunction& function);
bool peephole(MachineFunction& function);
std::string emit_assembly(MachineFunction const& function);

} // namespace jiterati::be::aarch64

#endif // JITERATI_BE_AARCH64_AARCH64_HPP_INCLUDED
