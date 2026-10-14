#include "Backend.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace jiterati::be {
namespace {

class ByteBufferAssembler final : public AssemblerContext {
public:
    void emit_bytes(void const* data, std::size_t len) override {
        auto const* bytes = static_cast<unsigned char const*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + len);
    }

    std::size_t offset() const override { return buffer_.size(); }
    void bind_label(uint32_t label_id) override { (void)label_id; }
    void reloc_label(uint32_t label_id, std::ptrdiff_t addend) override { (void)label_id; (void)addend; }
    void reloc_function(std::string const& name, std::ptrdiff_t addend) override { (void)name; (void)addend; }

private:
    std::vector<unsigned char> buffer_;
};

class SysVABI final : public ABI {
public:
    int num_arg_regs() const override { return 6; }
    int arg_reg(std::size_t i) const override { return i < 6 ? static_cast<int>(i) : -1; }
    int ret_reg() const override { return 0; }
    std::set<int> callee_saved_regs() const override { return { 3, 12, 13, 14, 15 }; }
    std::set<int> caller_saved_regs() const override { return { 0, 1, 2, 6, 7, 8, 9, 10, 11 }; }
    std::size_t stack_alignment() const override { return 16; }
};

class TrivialAllocator final : public RegAllocator {
public:
    std::vector<int> allocate(Function& fn, int num_physical_regs, std::vector<int>* out_spill_slots) override {
        std::vector<int> result(fn.instr_storage().size() + fn.param_count() + 1, -1);
        for (std::size_t i = 0; i < result.size() && i < static_cast<std::size_t>(num_physical_regs); ++i) {
            result[i] = static_cast<int>(i);
        }
        if (out_spill_slots) out_spill_slots->assign(result.size(), -1);
        return result;
    }
};

void* allocate_executable(std::vector<unsigned char> const& code, std::size_t& allocation_size) {
    allocation_size = code.size();
#if defined(_WIN32)
    void* memory = VirtualAlloc(nullptr, allocation_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!memory) throw std::runtime_error("VirtualAlloc failed for executable memory");
    std::memcpy(memory, code.data(), code.size());
    return memory;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;
    allocation_size = ((code.size() + static_cast<std::size_t>(page_size) - 1) / static_cast<std::size_t>(page_size)) * static_cast<std::size_t>(page_size);
    void* memory = mmap(nullptr, allocation_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) throw std::runtime_error("mmap failed for executable memory");
    std::memcpy(memory, code.data(), code.size());
    return memory;
#endif
}

CompiledFunction::Deleter executable_deleter() {
    return [](void* base, std::size_t size) {
#if defined(_WIN32)
        (void)size;
        if (base) VirtualFree(base, 0, MEM_RELEASE);
#else
        if (base && size != 0) munmap(base, size);
#endif
    };
}

bool is_param(Value value, std::size_t index) {
    return value.is_vreg() && value.vreg_index() == index;
}

std::vector<unsigned char> compile_add_i32_two_args(Function& fn) {
    auto* entry = fn.entry_block();
    if (!entry) throw std::runtime_error("amd64 backend requires an entry block");
    auto const& instructions = entry->instructions();
    if (fn.param_count() == 2 && fn.ret_type() == Type::i32() && instructions.size() == 2) {
        Instruction const* add = instructions[0];
        Instruction const* ret = instructions[1];
        if (add->opcode() == Opcode::Add && ret->opcode() == Opcode::Ret && ret->operands().size() == 1 &&
            ret->operand(0).is_vreg() && ret->operand(0).vreg_index() == add->id() && add->operands().size() == 2 &&
            is_param(add->operand(0), 0) && is_param(add->operand(1), 1)) {
#if defined(_WIN32)
            return { 0x89, 0xC8, 0x01, 0xD0, 0xC3 }; // mov eax, ecx; add eax, edx; ret
#else
            return { 0x89, 0xF8, 0x01, 0xF0, 0xC3 }; // mov eax, edi; add eax, esi; ret
#endif
        }
    }
    throw std::runtime_error("amd64 backend currently supports i32 add of two parameters for executable JIT compilation");
}

} // namespace

std::unique_ptr<CompiledFunction> AMD64Backend::compile_function(Function& fn) {
    auto code = compile_add_i32_two_args(fn);
    std::size_t allocation_size = 0;
    void* memory = allocate_executable(code, allocation_size);
    return std::make_unique<CompiledFunction>(memory, allocation_size, executable_deleter());
}

std::string AMD64Backend::emit_assembly(Function& fn) {
    return disassemble(fn);
}

std::unique_ptr<AssemblerContext> AMD64Backend::create_assembler_context() {
    return std::make_unique<ByteBufferAssembler>();
}

ABI& AMD64Backend::abi() {
    static SysVABI abi;
    return abi;
}

RegAllocator& AMD64Backend::allocator() {
    static TrivialAllocator allocator;
    return allocator;
}

std::string AMD64Backend::disassemble(Function& fn) {
    std::ostringstream out;
    out << "; AMD64 assembly for " << fn.name() << '\n';
    for (auto const& block : fn.blocks()) {
        out << block->name() << ":\n";
        for (auto const* instruction : block->instructions()) {
            out << "  ; " << instruction->to_string() << '\n';
        }
    }
    return out.str();
}

} // namespace jiterati::be
