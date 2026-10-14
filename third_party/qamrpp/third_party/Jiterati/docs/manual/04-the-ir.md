# The IR

Jiterati has two adjacent IR representations:
- **Core API IR** in `include/Jiterati.hpp`: builder-oriented, used by the C++ DSL and JIT path.
- **Terse/TAC IR** in `IR/IR.hpp`: parser/spec-oriented, used by textual IR, validation, and backend compiler interfaces.

## Terse IR
Terse IR models expression-oriented source structure:
- `TerseModule`
- `TerseFunction`
- `TerseExpr`
- `TerseOp`
- `UnaryOp`
- `BinaryOp`

Terse nodes represent literals, parameters, unary operations, binary operations, let bindings, conditionals, and calls.

## TAC IR
TAC IR canonicalizes expressions into instruction sequences:
- `TACModule`
- `TACFunction`
- `TACBlock`
- `TACInstruction`
- `TACValue`
- `TACOpcode`

TAC is the preferred validation and backend-facing representation.

## Scalar Types
`IR/IR.hpp` defines scalar types:
- `void`, `i1`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, `ptr`.

## Textual Grammar
The normative grammar is `specs/IR.ebnf`. The semantic specification is `specs/IR.md`.

Minimal shape:
```text
module demo;
func public i32 @add(i32 $lhs, i32 $rhs)
entry:
  %sum = add $lhs, $rhs;
  ret %sum;
end;
```

## Canonicalization
- Terse expressions lower to TAC instructions.
- TAC values name parameters, temporaries, immediates, labels, or globals.
- Validation should run after parsing/lowering and before backend compilation.
- Serialization must be deterministic.

## Compatibility
`specs/IR.md` is the language contract. `IR/Parse.cpp` is the implemented parser. When they differ, document the implementation gap rather than expanding examples beyond parser support.
