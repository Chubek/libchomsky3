# Chapter 12: Performance and Compile-Time Costs

## Runtime profile

- Feature composition uses static dispatch; no vtables.
- AST and parser layers allocate where needed (`std::vector`, `std::function`).
- `memoize` and `Lazy` can reduce repeated work at runtime.

## Compile-time profile

- NTTP-heavy and combinator-heavy code increases template instantiation load.
- Split large grammar definitions into reusable parser factories.
- Prefer focused rule sets over very broad predicates for rewrite passes.

Benchmark final domain code with representative inputs; the toolkit is flexible, but performance depends on your composition choices.
