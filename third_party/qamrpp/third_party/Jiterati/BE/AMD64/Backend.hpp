#ifndef JITERATI_BE_AMD64_BACKEND_HPP_INCLUDED
#define JITERATI_BE_AMD64_BACKEND_HPP_INCLUDED

#include "../../include/Jiterati-BE.hpp"

#include <cstddef>
#include <string>

namespace jiterati::be {

/// Concrete AMD64 (System V / x86-64) backend.
///
/// This backend lowers the core `jiterati::Function` IR directly to x86-64
/// machine code and returns a `CompiledFunction` backed by a region of
/// executable memory (allocated with `mmap`).  It is the path exercised by the
/// JIT entry point `JIT::compile(fn, backend)`.
///
/// The encoder supports a practical subset of the IR sufficient for integer
/// arithmetic, comparisons, control flow, memory access and intra-module calls:
///
///   - integer add / sub / mul / sdiv / udiv / srem / urem
///   - bitwise and / or / xor / shl / lshr / ashr / neg / not
///   - icmp (all signed/unsigned predicates) producing i1
///   - copy, const materialisation, select (cmov)
///   - load / store / alloca (stack-slot based)
///   - br / condbr (block labels)
///   - ret / call (intra-module, resolved when the callee is co-compiled)
///
/// Every value is spilled to a stack slot (`[rbp - 8*slot]`) and scratch
/// registers (rax, rcx, rdx, r10, r11) are used transiently.  This trades
/// performance for simplicity and correctness, which is the right trade-off for
/// a teaching/reference JIT.
class AMD64Backend : public Backend {
public:
    AMD64Backend() = default;

    std::string target_id() const override { return "amd64"; }

    std::unique_ptr<CompiledFunction> compile_function(Function& fn) override;

    std::string emit_assembly(Function& fn) override;

    std::unique_ptr<AssemblerContext> create_assembler_context() override;
    ABI& abi() override;
    RegAllocator& allocator() override;

private:
    // Internal textual-assembly emitter (used by emit_assembly() and as a
    // debugging aid).  Defined in Backend.cpp.
    std::string disassemble(Function& fn);
};

} // namespace jiterati::be

#endif // JITERATI_BE_AMD64_BACKEND_HPP_INCLUDED
