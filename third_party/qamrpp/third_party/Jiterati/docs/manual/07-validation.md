# Validation

Validation enforces IR invariants before transformation or lowering.

## Implemented Entry Points
```cpp
std::vector<Diagnostic> validate(TACFunction const& function);
std::vector<Diagnostic> validate(TACModule const& module);
```

Diagnostics are represented by `Diagnostic` in `IR/IR.hpp` and formatted by:
```cpp
std::string diagnostic_str(Diagnostic const& diagnostic);
```

## Core Invariants
- Defined symbols before use, except declared external references.
- Type-compatible operands and results.
- Terminator correctness per block.
- Branch targets resolve to labels.
- Function returns match declared return type.
- Calls match callee signature when known.
- Immediate values are legal for their declared type.

## CLI Validation
```sh
jiterati-cli validate input.ir
```

Expected behavior:
- parse the input;
- lower to TAC when needed;
- run validation;
- print diagnostics;
- exit nonzero on validation failure.

## Validation Placement
Run validation:
- after parsing;
- after lowering from Terse to TAC;
- after mutation-heavy transforms;
- before backend compilation;
- before serialization when malformed output would be harmful.

## Pass Interaction
Transforms should either preserve known invariants or invalidate affected analyses. A pass that mutates control flow must re-establish block terminators and successor consistency.
