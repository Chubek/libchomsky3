#ifndef JITERATI_BE_RV64_RV64_HPP_INCLUDED
#define JITERATI_BE_RV64_RV64_HPP_INCLUDED

#include "../../IR/IR.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace jiterati::be::rv64 {

// Integer registers follow the RV64I + standard LP64 calling convention.
// x0 (zero) is hardwired; ra/sp/gp/tp are reserved by the ABI.
enum class Reg {
    ZERO, RA, SP, GP, TP,
    T0, T1, T2,
    S0, S1,
    A0, A1, A2, A3, A4, A5, A6, A7,
    S2, S3, S4, S5, S6, S7, S8, S9, S10, S11,
    T3, T4, T5, T6,
    FT0, FT1, FT2, FT3, FT4, FT5, FT6, FT7,
    FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7,
};

enum class RegClass { Gpr, Fpr };

struct ABI {
    std::vector<Reg> integer_args;
    std::vector<Reg> float_args;
    Reg integer_return = Reg::A0;
    Reg float_return = Reg::FA0;
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
ABI lp64_abi();
RegClass reg_class(ir::Type type);
std::vector<Reg> allocatable_gprs();
std::vector<Reg> allocatable_fprs();

MachineFunction select(ir::TACFunction const& function);
void allocate_registers(MachineFunction& function);
void rewrite(MachineFunction& function);
bool peephole(MachineFunction& function);
std::string emit_assembly(MachineFunction const& function);

} // namespace jiterati::be::rv64

#endif // JITERATI_BE_RV64_RV64_HPP_INCLUDED
