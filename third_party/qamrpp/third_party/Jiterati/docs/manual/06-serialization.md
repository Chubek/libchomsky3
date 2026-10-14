# Serialization

Serialization converts in-memory IR to deterministic text or artifacts.

## Core API Serialization
Implemented core surface:
```cpp
std::string Module::to_string() const;
std::string Function::to_string() const;
std::string Instruction::to_string(Function const&) const;
```

JBL helpers:
```cpp
std::string print_jbl(Module const& module);
std::unique_ptr<Module> parse_jbl(std::string_view source, std::string* error);
```

## IR Serialization Contract
- Stable order for modules, functions, blocks, constants, and instructions.
- Explicit type spelling when ambiguity exists.
- No dependence on unordered container iteration order in user-visible output.
- Round-trip semantics over formatting preservation.

## CLI Formatting
```sh
jiterati-cli format input.ir
jiterati-cli format input.ir --out output.ir
```

`format` should parse, validate enough to avoid malformed output, and write normalized textual IR.

## Artifact Serialization
Backend compilers use:
```cpp
enum class ArtifactFormat { TextIR, Assembly, Object, Executable };
struct CompilationArtifact { ArtifactFormat format; std::vector<std::uint8_t> data; std::string diagnostics; };
```

`CompilationArtifact::ok()` reports whether diagnostics are empty.

## Failure Modes
- Nondeterministic emission.
- Loss of type information.
- Emitting parser-incompatible text.
- Treating comments/formatting as semantic content.
