# Chapter 1: Introduction

`DSLUtils.hpp` is a header-only C++20 toolkit for building domain-specific languages by composing feature mixins. You create one DSL type and opt into capabilities with `dsl::DSL<Derived, Features...>`.

## What this library is best at

- Building small internal DSLs without runtime polymorphism.
- Composing parsing, pattern dispatch, AST, rewrite, and expression-style APIs.
- Keeping call sites compact while preserving type safety.

## Feature map

Primary feature tags:

- `dsl::Pipeline`
- `dsl::Operators`
- `dsl::PatternMatch`
- `dsl::AST`
- `dsl::Rewrite`
- `dsl::ExprTemplates`
- `dsl::CustomLiterals`

Auxiliary facilities include memoization (`dsl::memoize`), deferred values (`dsl::Lazy<T>`), optional/result combinators (`dsl::Maybe`, `dsl::Result`), parser combinators (`dsl::Parser` family), and task orchestration (`dsl::Task`, `dsl::TaskChain`).

## First DSL

```cpp
#include "DSLUtils.hpp"

struct DemoDSL : dsl::DSL<DemoDSL, dsl::Pipeline, dsl::Operators> {};
```

This pattern is repeated throughout the manual: define one `Derived` type, add only the feature tags you need.
