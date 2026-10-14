#include "AMD64.hpp"

namespace jiterati::be::amd64 {

char const* reg_name(Reg reg) {
    switch (reg) {
        case Reg::RAX: return "rax"; case Reg::RCX: return "rcx"; case Reg::RDX: return "rdx";
        case Reg::RSI: return "rsi"; case Reg::RDI: return "rdi"; case Reg::R8: return "r8";
        case Reg::R9: return "r9"; case Reg::R10: return "r10"; case Reg::R11: return "r11";
        case Reg::RBX: return "rbx"; case Reg::R12: return "r12"; case Reg::R13: return "r13";
        case Reg::R14: return "r14"; case Reg::R15: return "r15"; case Reg::RBP: return "rbp";
        case Reg::RSP: return "rsp"; case Reg::XMM0: return "xmm0"; case Reg::XMM1: return "xmm1";
        case Reg::XMM2: return "xmm2"; case Reg::XMM3: return "xmm3"; case Reg::XMM4: return "xmm4";
        case Reg::XMM5: return "xmm5"; case Reg::XMM6: return "xmm6"; case Reg::XMM7: return "xmm7";
    }
    return "<?>";
}

ABI sysv_abi() {
    return {
        { Reg::RDI, Reg::RSI, Reg::RDX, Reg::RCX, Reg::R8, Reg::R9 },
        { Reg::XMM0, Reg::XMM1, Reg::XMM2, Reg::XMM3, Reg::XMM4, Reg::XMM5, Reg::XMM6, Reg::XMM7 },
        Reg::RAX,
        Reg::XMM0,
        { Reg::RAX, Reg::RCX, Reg::RDX, Reg::RSI, Reg::RDI, Reg::R8, Reg::R9, Reg::R10, Reg::R11 },
        { Reg::RBX, Reg::R12, Reg::R13, Reg::R14, Reg::R15 },
        16,
    };
}

RegClass reg_class(ir::Type type) {
    return type.is_float() ? RegClass::Fpr : RegClass::Gpr;
}

std::vector<Reg> allocatable_gprs() {
    return { Reg::RAX, Reg::RCX, Reg::RDX, Reg::RSI, Reg::RDI, Reg::R8, Reg::R9, Reg::R10, Reg::R11 };
}

std::vector<Reg> allocatable_fprs() {
    return { Reg::XMM0, Reg::XMM1, Reg::XMM2, Reg::XMM3, Reg::XMM4, Reg::XMM5, Reg::XMM6, Reg::XMM7 };
}

} // namespace jiterati::be::amd64
