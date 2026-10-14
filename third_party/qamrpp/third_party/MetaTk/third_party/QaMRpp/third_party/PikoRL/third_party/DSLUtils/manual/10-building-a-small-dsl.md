# Chapter 10: Building a Small DSL

A practical stack for small languages:

1. Token recognition with `pattern<...>` or parser primitives
2. Parse structure with combinators
3. Build AST with `leaf`/`node`
4. Simplify with rewrite rules
5. Evaluate or lower

Use one coordinating type, for example:

```cpp
struct CalcDSL : dsl::DSL<CalcDSL, dsl::PatternMatch, dsl::AST, dsl::Rewrite> {
  static inline auto rules = dsl::rewrite_set(/* ... */);
};
```

This keeps the user-facing API small while still allowing staged compilation-like processing.
