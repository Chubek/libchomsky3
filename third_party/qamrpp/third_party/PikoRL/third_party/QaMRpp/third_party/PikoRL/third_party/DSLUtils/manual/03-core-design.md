# Chapter 3: Core Design

## CRTP composition model

`dsl::DSL<Derived, Features...>` inherits `Features::Mixin<Derived>...`. This gives static composition with zero virtual dispatch.

```cpp
struct Lang : dsl::DSL<Lang, dsl::Pipeline, dsl::AST> {};
```

## Fixed compile-time strings

`dsl::FixedString` enables string NTTPs for APIs like `leaf<"num">`, `node<"add">`, and `pattern<"[0-9]+">`.

## Concepts for constraints

Useful concepts exposed by the header:

- `dsl::HasFeature<T, F>`
- `dsl::Pipeable<T>`
- `dsl::ExprLike<T>`
- `dsl::HasLiterals<T>`
- `dsl::Rewritable<T>`

Use these in your own templates to produce clearer diagnostics when a DSL feature is missing.
