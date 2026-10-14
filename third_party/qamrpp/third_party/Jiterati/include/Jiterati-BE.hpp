/** @file Jiterati-BE.hpp
 *  @brief Backend abstraction layer for Jiterati.
 *
 *  Defines the interfaces that every backend (AMD64, AArch64, RV64, WASM, ...)
 *  must implement.  Concrete backends live in BE/ subdirectories.
 */
#ifndef JITERATI_BE_HPP_INCLUDED
#define JITERATI_BE_HPP_INCLUDED

#include "Jiterati.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
#include <set>
#include <unordered_map>

namespace jiterati {

// -----------------------------------------------------------------------------
// CompiledFunction — result of backend compilation
// -----------------------------------------------------------------------------

/** Owns a region of executable memory and the associated metadata. */
class CompiledFunction {
public:
    using Deleter = std::function<void(void* base, std::size_t size)>;

    CompiledFunction(void* base, std::size_t size, Deleter deleter)
        : base_(base), size_(size), deleter_(std::move(deleter)) {}

    ~CompiledFunction() {
        if (base_ && deleter_) {
            deleter_(base_, size_);
        }
    }

    // Movable only
    CompiledFunction(CompiledFunction const&) = delete;
    CompiledFunction& operator=(CompiledFunction const&) = delete;

    CompiledFunction(CompiledFunction&& o) noexcept
        : base_(o.base_), size_(o.size_), deleter_(std::move(o.deleter_)) {
        o.base_ = nullptr;
        o.size_ = 0;
    }
    CompiledFunction& operator=(CompiledFunction&& o) noexcept {
        if (this != &o) {
            if (base_ && deleter_) deleter_(base_, size_);
            base_ = o.base_;
            size_ = o.size_;
            deleter_ = std::move(o.deleter_);
            o.base_ = nullptr;
            o.size_ = 0;
        }
        return *this;
    }

    void* base() const { return base_; }
    void* entry() const { return base_; }
    std::size_t size() const { return size_; }

    template <typename T>
    T as() const { return reinterpret_cast<T>(base_); }

private:
    void* base_ = nullptr;
    std::size_t size_ = 0;
    Deleter deleter_;
};

// -----------------------------------------------------------------------------
// Low-level assembler context
// -----------------------------------------------------------------------------

/** Per-backend assembler context used during code emission.  It is responsible
 *  for encoding instructions, managing labels, and recording relocations. */
class AssemblerContext {
public:
    virtual ~AssemblerContext() = default;

    /** Append raw bytes. */
    virtual void emit_bytes(const void* data, std::size_t len) = 0;

    /** Return the current offset in bytes. */
    virtual std::size_t offset() const = 0;

    /** Bind a label to the current offset. */
    virtual void bind_label(uint32_t label_id) = 0;

    /** Emit a relocation to a label. */
    virtual void reloc_label(uint32_t label_id, std::ptrdiff_t addend) = 0;

    /** Emit a relocation to a function name (for external calls). */
    virtual void reloc_function(std::string const& name, std::ptrdiff_t addend) = 0;

    template <typename T>
    void emit(T value) {
        static_assert(std::is_trivially_copyable_v<T>);
        emit_bytes(&value, sizeof(T));
    }
};

// -----------------------------------------------------------------------------
// Register allocation interface
// -----------------------------------------------------------------------------

/** Abstract register allocator.  Implementations may be simple (linear scan)
 *  or more sophisticated (graph coloring, SSA-based). */
class RegAllocator {
public:
    virtual ~RegAllocator() = default;

    /** Allocate physical registers for the given function.  Returns a mapping
     *  from virtual register index to physical register index.  Spilled values
     *  are indicated by a sentinel (e.g. -1). */
    virtual std::vector<int> allocate(Function& fn,
                                       int num_physical_regs,
                                       std::vector<int>* out_spill_slots) = 0;
};

// -----------------------------------------------------------------------------
// ABI interface
// -----------------------------------------------------------------------------

/** Describes a calling convention for a target. */
class ABI {
public:
    virtual ~ABI() = default;

    /** Number of caller-saved / volatile registers. */
    virtual int num_arg_regs() const = 0;

    /** Returns the physical register index used for argument i (or -1 if
     *  passed on stack). */
    virtual int arg_reg(std::size_t i) const = 0;

    /** Register used for integer return values. */
    virtual int ret_reg() const = 0;

    /** Registers that must be preserved across calls. */
    virtual std::set<int> callee_saved_regs() const = 0;

    /** Registers that are volatile / caller-saved. */
    virtual std::set<int> caller_saved_regs() const = 0;

    /** Stack alignment in bytes required for calls. */
    virtual std::size_t stack_alignment() const = 0;
};

// -----------------------------------------------------------------------------
// Backend interface
// -----------------------------------------------------------------------------

/** A backend turns Jiterati IR into executable code or target-specific bytes. */
class Backend {
public:
    virtual ~Backend() = default;

    /** Target identifier, e.g. "amd64", "aarch64", "rv64", "wasm". */
    virtual std::string target_id() const = 0;

    /** Compile an entire function. */
    virtual std::unique_ptr<CompiledFunction> compile_function(Function& fn) = 0;

    /** Optional: emit target-specific textual assembly instead of machine code. */
    virtual std::string emit_assembly(Function& fn) {
        (void)fn;
        return "";
    }

    /** Create a fresh assembler context for emission tests. */
    virtual std::unique_ptr<AssemblerContext> create_assembler_context() = 0;

    /** Access the ABI used by this backend. */
    virtual ABI& abi() = 0;

    /** Access the register allocator used by this backend. */
    virtual RegAllocator& allocator() = 0;
};

// -----------------------------------------------------------------------------
// Backend helpers
// -----------------------------------------------------------------------------

/** Split an integer signed-divide into an instruction sequence compatible with
 *  architectures that do not have a direct remainder instruction.  The returned
 *  vector contains lowered pseudo-opcodes understood by the simple backends in
 *  this project.  For now it is a documentation placeholder. */
inline std::vector<Opcode> lower_divrem_sequence(Opcode op, Type ty) {
    (void)ty;
    if (op == Opcode::SRem) return { Opcode::SDiv, Opcode::Mul, Opcode::Sub };
    if (op == Opcode::URem) return { Opcode::UDiv, Opcode::Mul, Opcode::Sub };
    return { op };
}

} // namespace jiterati

#endif // JITERATI_BE_HPP_INCLUDED
