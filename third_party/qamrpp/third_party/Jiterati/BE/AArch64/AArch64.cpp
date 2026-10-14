#include "AArch64.hpp"

namespace jiterati::be::aarch64 {

char const* reg_name(Reg reg) {
    switch (reg) {
        case Reg::X0: return "x0"; case Reg::X1: return "x1"; case Reg::X2: return "x2"; case Reg::X3: return "x3";
        case Reg::X4: return "x4"; case Reg::X5: return "x5"; case Reg::X6: return "x6"; case Reg::X7: return "x7";
        case Reg::X8: return "x8"; case Reg::X9: return "x9"; case Reg::X10: return "x10"; case Reg::X11: return "x11";
        case Reg::X12: return "x12"; case Reg::X13: return "x13"; case Reg::X14: return "x14"; case Reg::X15: return "x15";
        case Reg::X16: return "x16"; case Reg::X17: return "x17"; case Reg::X19: return "x19"; case Reg::X20: return "x20";
        case Reg::X21: return "x21"; case Reg::X22: return "x22"; case Reg::X23: return "x23"; case Reg::X24: return "x24";
        case Reg::X25: return "x25"; case Reg::X26: return "x26"; case Reg::X27: return "x27"; case Reg::X28: return "x28";
        case Reg::FP: return "x29"; case Reg::LR: return "x30"; case Reg::SP: return "sp";
        case Reg::V0: return "v0"; case Reg::V1: return "v1"; case Reg::V2: return "v2"; case Reg::V3: return "v3";
        case Reg::V4: return "v4"; case Reg::V5: return "v5"; case Reg::V6: return "v6"; case Reg::V7: return "v7";
    }
    return "<?>";
}

ABI aapcs64_abi() {
    return {
        { Reg::X0, Reg::X1, Reg::X2, Reg::X3, Reg::X4, Reg::X5, Reg::X6, Reg::X7 },
        { Reg::V0, Reg::V1, Reg::V2, Reg::V3, Reg::V4, Reg::V5, Reg::V6, Reg::V7 },
        Reg::X0, Reg::V0,
        { Reg::X0, Reg::X1, Reg::X2, Reg::X3, Reg::X4, Reg::X5, Reg::X6, Reg::X7, Reg::X8, Reg::X9, Reg::X10, Reg::X11, Reg::X12, Reg::X13, Reg::X14, Reg::X15, Reg::X16, Reg::X17 },
        { Reg::X19, Reg::X20, Reg::X21, Reg::X22, Reg::X23, Reg::X24, Reg::X25, Reg::X26, Reg::X27, Reg::X28 },
        16,
    };
}

RegClass reg_class(ir::Type type) { return type.is_float() ? RegClass::Fpr : RegClass::Gpr; }
std::vector<Reg> allocatable_gprs() { return { Reg::X9, Reg::X10, Reg::X11, Reg::X12, Reg::X13, Reg::X14, Reg::X15, Reg::X16, Reg::X17 }; }
std::vector<Reg> allocatable_fprs() { return { Reg::V0, Reg::V1, Reg::V2, Reg::V3, Reg::V4, Reg::V5, Reg::V6, Reg::V7 }; }

} // namespace jiterati::be::aarch64
