#ifndef JITERATI_BE_AMD64_AMD64_HPP_INCLUDED
#define JITERATI_BE_AMD64_AMD64_HPP_INCLUDED

#include "../../IR/IR.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace jiterati::be::amd64 {

enum class Reg {
    RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11,
    RBX, R12, R13, R14, R15, RBP, RSP,
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
};

enum class RegClass {
    Gpr,
    Fpr,
};

struct ABI {
    std::vector<Reg> integer_args;
    std::vector<Reg> float_args;
    Reg integer_return = Reg::RAX;
    Reg float_return = Reg::XMM0;
    std::vector<Reg> caller_saved;
    std::vector<Reg> callee_saved;
    std::size_t stack_alignment = 16;
};

struct MachineInst {
    std::string opcode;
    std::vector<std::string> operands;
    std::string comment;
};

struct MachineBlock {
    std::string label;
    std::vector<MachineInst> instructions;
};

struct MachineFunction {
    std::string name;
    std::vector<MachineBlock> blocks;
    std::map<std::string, Reg> vreg_assignment;
    std::size_t stack_size = 0;
};

char const* reg_name(Reg reg);
ABI sysv_abi();
RegClass reg_class(ir::Type type);
std::vector<Reg> allocatable_gprs();
std::vector<Reg> allocatable_fprs();

MachineFunction select(ir::TACFunction const& function);
void allocate_registers(MachineFunction& function);
void rewrite(MachineFunction& function);
bool peephole(MachineFunction& function);
std::string emit_assembly(MachineFunction const& function);

} // namespace jiterati::be::amd64

#endif // JITERATI_BE_AMD64_AMD64_HPP_INCLUDED
