# Internal Design

Jiterati internals are organized to keep semantic representation independent from target lowering.

## Core Storage
- `Module` stores functions.
- `Function` stores blocks, instructions, parameters, constants.
- `Block` stores ordered instruction references and CFG edges.
- `Value` is an index handle, not an owning object.

## IR Library Storage
- Terse IR preserves expression structure.
- TAC IR normalizes expressions into explicit temporaries and instructions.
- Diagnostics travel beside parse/validate results.

## Backend Internals
Each target should isolate:
- instruction selection;
- register allocation;
- illegal-form rewriting;
- peephole cleanup;
- emission;
- artifact wrapping.

## Determinism
Required for:
- textual serialization;
- diagnostics order;
- package manifests;
- generated backend tables;
- test fixtures.

## Dependency Policy
- Core API stays C++17 and lightweight.
- CLI uses standard library facilities plus vendored Zstandard helpers.
- Lua bindings depend on Sol2/Lua and are optional through CMake.
- CLI11 is vendored but current CLI implementation is manually parsed.

## Maintenance Rules
- Do not make specifications chase accidental implementation behavior.
- Do not introduce architecture facts outside backend/dataset boundaries.
- Prefer generated backend metadata over duplicated tables.
- Add tests for user-visible behavior.
