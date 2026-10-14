# Backend Interface

Backends convert validated IR to executable code or artifacts.

## Core JIT Backend API
`include/Jiterati-BE.hpp` defines:
```cpp
class Backend {
public:
    virtual std::string name() const = 0;
    virtual std::unique_ptr<CompiledFunction> compile_function(Function&) = 0;
};
```

Related abstractions:
- `CompiledFunction`: executable function pointer plus optional owned memory.
- `AssemblerContext`: byte buffer and primitive emitters.
- `RegAllocator`: virtual-register to physical-register mapping.
- `ABI`: argument and return-register policy.

## IR Compiler API
`IR/ICompiler.hpp` defines artifact compilation:
```cpp
struct CompileOptions { std::string target; ArtifactFormat format; std::string entry; };
class ICompiler { virtual CompilationArtifact compile_tac(TACModule const&, CompileOptions) = 0; };
```

Factory functions:
- `create_amd64_ir_compiler()`
- `create_aarch64_ir_compiler()`
- `create_rv64_ir_compiler()`
- `create_wasm_ir_compiler()`

## Backend Metadata
`BE/Metadata.hpp` exposes dataset-derived descriptors:
```cpp
BackendDatasetMetadata const* backend_dataset_metadata_begin();
BackendDatasetMetadata const* backend_dataset_metadata_end();
BackendDatasetMetadata const* find_backend_dataset_metadata(std::string_view target);
```

## CLI Compilation
```sh
jiterati-cli compile --target amd64 input.ir --emit asm --out output.s
```

Supported target names should track backend metadata and dataset names.

## Correctness Boundary
Generic code owns semantic validation. Backends own legality, lowering, register allocation, calling convention details, instruction encoding, and artifact emission.
