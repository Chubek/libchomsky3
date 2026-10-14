#include "RV64.hpp"

namespace jiterati::be::rv64 {

char const* reg_name(Reg r) {
    switch (r) {
        case Reg::ZERO: return "zero"; case Reg::RA: return "ra"; case Reg::SP: return "sp";
        case Reg::GP: return "gp"; case Reg::TP: return "tp";
        case Reg::T0: return "t0"; case Reg::T1: return "t1"; case Reg::T2: return "t2";
        case Reg::S0: return "s0"; case Reg::S1: return "s1";
        case Reg::A0: return "a0"; case Reg::A1: return "a1"; case Reg::A2: return "a2";
        case Reg::A3: return "a3"; case Reg::A4: return "a4"; case Reg::A5: return "a5";
        case Reg::A6: return "a6"; case Reg::A7: return "a7";
        case Reg::S2: return "s2"; case Reg::S3: return "s3"; case Reg::S4: return "s4";
        case Reg::S5: return "s5"; case Reg::S6: return "s6"; case Reg::S7: return "s7";
        case Reg::S8: return "s8"; case Reg::S9: return "s9"; case Reg::S10: return "s10";
        case Reg::S11: return "s11";
        case Reg::T3: return "t3"; case Reg::T4: return "t4"; case Reg::T5: return "t5"; case Reg::T6: return "t6";
        case Reg::FT0: return "ft0"; case Reg::FT1: return "ft1"; case Reg::FT2: return "ft2";
        case Reg::FT3: return "ft3"; case Reg::FT4: return "ft4"; case Reg::FT5: return "ft5";
        case Reg::FT6: return "ft6"; case Reg::FT7: return "ft7";
        case Reg::FA0: return "fa0"; case Reg::FA1: return "fa1"; case Reg::FA2: return "fa2";
        case Reg::FA3: return "fa3"; case Reg::FA4: return "fa4"; case Reg::FA5: return "fa5";
        case Reg::FA6: return "fa6"; case Reg::FA7: return "fa7";
    }
    return "<?>";
}

ABI lp64_abi() {
    ABI abi;
    abi.integer_args = { Reg::A0, Reg::A1, Reg::A2, Reg::A3, Reg::A4, Reg::A5, Reg::A6, Reg::A7 };
    abi.float_args   = { Reg::FA0, Reg::FA1, Reg::FA2, Reg::FA3, Reg::FA4, Reg::FA5, Reg::FA6, Reg::FA7 };
    abi.integer_return = Reg::A0;
    abi.float_return   = Reg::FA0;
    // Caller-saved (temporary) registers: ra, the t* scratch set, and the a* argument set.
    abi.caller_saved = { Reg::RA, Reg::T0, Reg::T1, Reg::T2, Reg::T3, Reg::T4, Reg::T5, Reg::T6,
                         Reg::A0, Reg::A1, Reg::A2, Reg::A3, Reg::A4, Reg::A5, Reg::A6, Reg::A7 };
    // Callee-saved (preserved) registers: the s* set (s0 doubles as the frame pointer).
    abi.callee_saved = { Reg::S0, Reg::S1, Reg::S2, Reg::S3, Reg::S4, Reg::S5, Reg::S6, Reg::S7,
                         Reg::S8, Reg::S9, Reg::S10, Reg::S11 };
    abi.stack_alignment = 16;
    return abi;
}

RegClass reg_class(ir::Type type) {
    return type.is_float() ? RegClass::Fpr : RegClass::Gpr;
}

std::vector<Reg> allocatable_gprs() {
    return { Reg::T0, Reg::T1, Reg::T2, Reg::T3, Reg::T4, Reg::T5, Reg::T6,
             Reg::A0, Reg::A1, Reg::A2, Reg::A3, Reg::A4, Reg::A5, Reg::A6, Reg::A7 };
}

std::vector<Reg> allocatable_fprs() {
    return { Reg::FT0, Reg::FT1, Reg::FT2, Reg::FT3, Reg::FT4, Reg::FT5, Reg::FT6, Reg::FT7,
             Reg::FA0, Reg::FA1, Reg::FA2, Reg::FA3, Reg::FA4, Reg::FA5, Reg::FA6, Reg::FA7 };
}

} // namespace jiterati::be::rv64
