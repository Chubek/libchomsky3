# Writing A Backend

A backend must implement target lowering without leaking machine semantics into generic IR.

## Required Inputs
- Target dataset under `.agents/datasets/`.
- Backend metadata entry in `BE/Metadata.cpp`.
- Compiler factory in `IR/ICompiler.hpp` implementation path.
- Stage files matching existing layout when practical.

## Recommended File Layout
```text
BE/<Target>/
  <Target>.hpp/.cpp
  ISel.hpp/.cpp
  RegAlloc.hpp/.cpp
  Rewrite.hpp/.cpp
  Peephole.hpp/.cpp
  Emit.hpp/.cpp
  IRCompiler.cpp
```

## Implementation Order
1. Define target metadata.
2. Implement TAC legality checks.
3. Select semantic operations into target operations.
4. Allocate registers according to ABI constraints.
5. Rewrite illegal forms.
6. Run target peepholes.
7. Emit assembly/object bytes.
8. Add tests and examples.

## Dataset Rules
- Registers, aliases, ABI rules, instruction names, encodings, features, relocations, and addressing modes come from the matching file in `.agents/datasets/`.
- Manual backend tables must be justified by algorithmic need or missing generated infrastructure.
- Generated tables must be deterministic.

## Target Responsibilities
- Lower unsupported integer widths.
- Legalize immediates and addressing modes.
- Preserve calling convention semantics.
- Respect callee/caller-saved registers.
- Model relocations explicitly.
- Report unsupported artifacts rather than emitting invalid output.

## Failure Modes
- Duplicated architecture facts diverging from datasets.
- Instruction selection before type/legalization checks.
- ABI policy embedded in generic code.
- Peepholes changing observable semantics.
