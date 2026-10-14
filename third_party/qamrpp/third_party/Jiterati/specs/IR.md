# Jiterati IR Specification

## Design Philosophy

Jiterati IR is a small, typed intermediate representation intended for compiler experimentation, JIT embedding, and backend prototyping. The IR should remain easy to construct from C++, easy to serialize for debugging, and strict enough that backends can reject invalid programs before target lowering.

The implementation currently exposes public API types in `include/Jiterati.hpp` and a richer terse/TAC model in `IR/IR.hpp`. This specification defines the intended behavior independently of those implementation details.

## Object Model

A module owns functions, declarations, globals, metadata, and package-facing symbol information. A function owns parameters, blocks, instructions, and local temporaries. A block owns an ordered instruction list and must end in a terminator once validation succeeds. Instructions may produce a result value, consume operands, reference symbols, and carry metadata.

Values are typed. A value may refer to a parameter, temporary, instruction result, or constant pool entry. The textual spelling distinguishes local values with `%`, parameters with `$`, labels with `^`, and global symbols with `@`.

## Types

Primitive types are `void`, `i1`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, and `ptr`. Integer operations require integer operands of identical width unless an instruction explicitly defines extension, truncation, or reinterpretation behavior. Floating-point operations require matching floating-point widths. `void` may only appear as a function return type or as the type of instructions that do not produce a value.

## Instruction Categories

- Constants materialize typed literals.
- Arithmetic instructions operate on integer or floating-point values.
- Bitwise instructions operate on integer values.
- Comparison instructions return `i1`.
- Control-flow instructions transfer execution to labels.
- Calls reference global symbols and obey declared parameter and return types.
- Phi instructions merge values from predecessor blocks.
- Backend intrinsics describe target-specific operations and must be validated by the selected backend.

## SSA Rules

Each result name is defined exactly once within a function. A use must be dominated by its definition except for phi operands, which are interpreted along incoming edges. Parameters are considered defined at function entry. Constants may be shared and do not participate in dominance.

## Symbol Visibility

`public` symbols are exported by a module or package. `private` symbols are module-local. `internal` symbols may be optimized across module boundaries only when the package loader proves there is no external reference. `external` symbols are declarations resolved by the embedding process, linker, plugin loader, or runtime.

## Naming Rules

Identifiers are case-sensitive and must match the grammar in `specs/IR.ebnf`. Names beginning with implementation-reserved prefixes should not be emitted by user code. Serializers should preserve user names when valid and synthesize deterministic names when missing.

## Validation Rules

Validation must check type compatibility, unique definitions, valid block labels, terminator placement, branch target existence, phi predecessor consistency, call signature compatibility, global initializer compatibility, and metadata syntax. Backend validation additionally checks instruction availability, operand forms, register classes, relocation model support, and feature requirements.

## Serialization Rules

Serialization must be deterministic. Modules, globals, functions, blocks, and metadata should be emitted in stable order. Round-tripping through parse and serialize must preserve semantics, though comments and formatting may be normalized. Textual IR should prefer explicit types where ambiguity would otherwise exist.

## Textual Example

```text
module demo;

func public i32 @add(i32 $lhs, i32 $rhs)
entry:
  %sum = add $lhs, $rhs;
  ret %sum;
end;
```

## Diagnostics

Diagnostics should include severity, source location when available, a concise message, and optional notes or suggestions. Error text should be stable enough for tests when practical.
