# Transformation Passes

Transformation passes mutate IR while preserving semantics.

## Bundled Transforms
`Passlib/` contains:
- `ConstantFolding.cpp`
- `ConstantPropagation.cpp`
- `DeadCodeRemoval.cpp`
- `StrengthReduction.cpp`
- `PeepholeOptimization.cpp`

## Transform Contract
- Maintain valid ownership and operand references.
- Preserve function type signatures unless explicitly performing ABI-level rewriting.
- Rebuild or invalidate analyses affected by mutation.
- Preserve deterministic output.
- Validate after structural rewrites.

## Value Rewriting
TAC helper:
```cpp
void rewrite_values(TACFunction& function, TACValue const& from, TACValue const& to);
```

Peephole helpers:
```cpp
bool peephole(TACFunction& function);
bool peephole(TACModule& module);
```

## Pipeline Guidance
Recommended order:
1. canonicalization;
2. constant folding/propagation;
3. dead-code removal;
4. strength reduction;
5. peepholes;
6. validation.

## Failure Modes
- Replacing values without updating all uses.
- Removing instructions still used by later instructions.
- Invalidating control-flow metadata silently.
- Introducing target instructions into semantic IR.
