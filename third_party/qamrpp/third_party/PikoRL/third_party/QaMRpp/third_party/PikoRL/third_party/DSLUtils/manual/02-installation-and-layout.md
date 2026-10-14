# Chapter 2: Installation and Repository Layout

## Requirements

- C++20 compiler
- Standard library with `<optional>`, `<variant>`, `<functional>`, `<string_view>`

The library is header-only. Include `DSLUtils.hpp` directly or from your own wrapper header.

## Typical wrapper header

```cpp
#pragma once
#include "DSLUtils.hpp"

struct MyDSL : dsl::DSL<MyDSL, dsl::Pipeline, dsl::AST> {};
```

## Layout guidance

- Keep domain-specific rules (rewrite rules, parser definitions, literal sets) in your own namespace/module.
- Keep `DSLUtils.hpp` as infrastructure.
- Prefer small translation units for examples and tests; compile-time-heavy code benefits from isolation.
