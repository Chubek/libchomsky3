# Core Concepts

## Runtime IR
The header-only core API in `include/Jiterati.hpp` exposes:
- `Module`: owns functions.
- `Function`: owns blocks, instructions, parameters, constants.
- `Block`: ordered instruction list plus control-flow successors.
- `Instruction`: opcode, result type, operands.
- `Value`: SSA handle, constant handle, or invalid sentinel.
- `Type`: primitive scalar type wrapper.

## Primitive Types
| Type | Meaning |
|---|---|
| `void` | no value |
| `i1` | boolean/integer predicate |
| `i8`, `i16`, `i32`, `i64` | fixed-width integers |
| `f32`, `f64` | floating scalars |
| `ptr` | target pointer |

## Value Model
`Value` uses a compact tagged index:
- `0`: invalid/none.
- positive: virtual register or instruction result.
- negative: constant-pool index.

## Instruction Model
Core opcodes cover:
- constants and moves;
- integer and floating arithmetic;
- bitwise operations;
- comparisons;
- select;
- memory operations;
- calls;
- returns and branches.

## Builder Discipline
- Create a module.
- Create functions with explicit return and parameter types.
- Create blocks through the owning function.
- Append instructions through block builders.
- Keep terminators explicit.
- Validate before lowering or execution.

## Ownership
- Modules own functions.
- Functions own blocks, constants, and instructions.
- Blocks reference instructions and successors.
- Public handles are lightweight; do not infer ownership from raw pointers.

## Implemented Examples
Use `examples/API/*.cpp` as the current source for buildable API patterns. Use `examples/IR/*.ir` for textual IR patterns.
