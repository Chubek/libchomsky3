# DSLUtils: Header-Only Tooklit Creating C++-Native DSLs in Your Application

A modern C++20 header-only library for building domain-specific languages (DSLs) with composable, type-safe features.

## Overview

`DSLUtils.hpphpp` provides a comprehensive toolkit for creating embedded DSLs in C++. It offers a CRTP-based architecture that allows you to mix and match features to build expressive, type-safe APIs tailored to your domain.

## Features

- **Pipeline DSL** (`dsl::Pipeline`) - Fluent operator `|` chaining for data transformation
- **Composable Operators** (`dsl::Operators`) - Predicate combinators with `&&`, `||`, `!`
- **Pattern Matching** (`dsl::PatternMatch`) - Compile-time pattern matching with `match`/`when`/`otherwise`
- **AST Construction** (`dsl::AST`) - Value-type abstract syntax trees with `leaf`/`node`/`dump`
- **Rewrite Rules** (`dsl::Rewrite`) - AST transformation with `rule`/`rewrite_set`/`apply`
- **Expression Templates** (`dsl::ExprTemplates`) - Lazy expression trees for deferred evaluation
- **Custom Literals** (`dsl::CustomLiterals`) - User-defined literal suffix registration and parsing
- **Memoization** (`dsl::Memoization`) - Automatic caching for expensive pure functions
- **Lazy Evaluation** (`dsl::Lazy`) - Deferred computation with `Lazy<T>` wrapper
- **Monadic Operations** (`dsl::Monadic`) - Functional `map`/`flat_map` for `Maybe<T>`
- **Result Type** (`dsl::Result`) - Explicit error handling with `Result<T, E>`
- **Parser Combinators** (`dsl::CombinatorParser`) - Functional parser DSL with semantic hooks

## Quick Start
```cpp
#include "DSLUtils.hpphpp"

// Example: Units DSL
struct UnitsDSL : dsl::DSL<UnitsDSL, dsl::Pipeline> {
double value;
explicit UnitsDSL(double v) : value(v) {}
};

auto meters(double v) { return UnitsDSL(v); }
auto to_feet = [](const UnitsDSL& u) { return UnitsDSL(u.value * 3.28084); };

int main() {
auto result = meters(10.0) | to_feet;
// result.value == 32.8084
}
```
## Documentation

**Full API documentation:** [dslutils-docs.pdf](http://warble.ir:8000/dslutils-docs.pdf)

The PDF includes:
- Detailed feature reference
- Complete API documentation with Doxygen comments
- Multiple working examples (units, state machines, vectors, queries, parsers)
- Implementation notes and design rationale

To generate documentation in other formats, just run `doxygen`. It supports XML and HTML.

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Standard library with concepts support

## Installation

`DSLUtils.hpphpp` is header-only. Simply copy the file to your project:

```bash
# Download
curl -O https://raw.githubusercontent.com/Chubek/DSLUtils.hppcpp/refs/heads/master/DSLUtils.hpphpp

# Or add as submodule
git submodule add https://github.com/Chubek/DSLUtils.hpp.git external/dslutils
```
Then include in your code:

```cpp
#include "DSLUtils.hpphpp"
```
## Examples

The library includes several complete examples in the `examples/` directory:

- **`units-dsl.cpp`** - Physical units with automatic conversion
- **`state-machine.cpp`** - Declarative state machine DSL
- **`vector-ops.cpp`** - Mathematical vector operations
- **`query-dsl.cpp`** - SQL-like query builder
- **`awk-parser.cpp`** - Parser combinator demonstration (Awk subset)
- **`lazy-eval.cpp`** - Deferred computation examples
- **`result-monad.cpp`** - Error handling with Result type
- **`memoization.cpp`** - Function caching demonstration

Build examples:

```bash
g++ -std=c++20 -O2 examples/units-dsl.cpp -o units-dsl
./units-dsl
```
## Core Concepts

### Feature Composition

Build DSLs by inheriting from `dsl::DSL` and selecting features:

```cpp
struct MyDSL : dsl::DSL<MyDSL, 
dsl::Pipeline,
dsl::Operators,
dsl::PatternMatch> {
// Your domain logic
};
```
### Type Safety

All operations are checked at compile-time using C++20 concepts:

```cpp
template<typename T>
concept Pipeable = requires(T t) {
{ t | std::declval<PipeStage<T>>() } -> std::same_as<T>;
};
```

### Zero-Cost Abstractions

Expression templates and compile-time evaluation ensure no runtime overhead:

```cpp
auto expr = x + y * z;  // No evaluation yet
auto result = expr.eval(ctx);  // Single evaluation pass
```

## Advanced Features

### Composable DSL Architectures

Combine multiple DSL capabilities into a single domain model.
```cpp
struct QueryDSL :
dsl::DSL<
QueryDSL,
dsl::Pipeline,
dsl::Operators,
dsl::PatternMatch
> {

std::string query;
};
```
This allows fluent APIs that integrate:

- pipelines
- predicates
- matching
- expression composition

without runtime overhead.

---

### AST Rewriting Pipelines

Transform syntax trees using declarative rewrite systems.

```cpp
auto rules = rewrite_set(
rule(is_constant_addition, fold_constants),
rule(is_mul_by_one, remove_identity),
rule(is_mul_by_zero, fold_zero)
);

auto optimized = apply(rules, ast);
```
Rewrite systems repeatedly apply transformations until a fixpoint is reached.

Useful for:

- optimizers
- compilers
- interpreters
- query engines
- symbolic algebra

---

### Expression Fusion

Expression templates eliminate temporary allocations.

```cpp
auto expr =
(x + y)
* (z - w)
/ scale;
```
Evaluation is deferred and fused into a single computation pass.

Benefits:

- zero temporary objects
- reduced memory traffic
- compile-time optimization
- SIMD-friendly evaluation

---

### Semantic Parser Hooks

Parser combinators support semantic actions through production IDs.

```cpp
auto grammar =
production("assignment"_id,
identifier
& token("=")
& expression);
```
Semantic handlers may be attached using:

- direct callback functions
- dispatch tables
- parser stage processors

Example:

```cpp
handler_table["assignment"_id] =
[](const ParserStage& stage) {
build_assignment_node(stage);
};
```
---

### Lazy Evaluation Graphs

Deferred computations may be chained into execution graphs.

```cpp
Lazy<int> a = [] { return compute_a(); };
Lazy<int> b = [] { return compute_b(); };

auto result =
a.map([](int x) { return x * 2; })
.flat_map([&](int x) {
return b.map([=](int y) {
return x + y;
});
});
```
Evaluation only occurs when values are requested.

---

### Monadic Error Propagation

`Result<T, E>` enables composable error handling.

```cpp
auto result =
read_file(path)
.flat_map(parse_json)
.flat_map(validate_schema)
.map(build_object);
```
Errors propagate automatically without exceptions.

Useful for:

- parsers
- compilers
- configuration systems
- networking
- serialization

---

### Memoized Functional Pipelines

Combine memoization with lazy evaluation for efficient computation graphs.

```cpp
auto expensive =
memoize([](int n) {
return fibonacci(n);
});

Lazy<int> value = [=] {
return expensive(40);
};
```
Repeated evaluations reuse cached results.

---

### Custom Literal Systems

Register literal suffixes dynamically.

```cpp
literal_set literals;

literals.add("ms",
[](std::string_view v) {
return Duration(std::stoi(std::string(v)));
});

auto duration =
parse_literal("250ms", literals);
```
Useful for:

- units systems
- configuration DSLs
- scripting languages
- scientific notation

---

### Pattern-Driven Dispatch

Pattern matching enables concise compile-time dispatch.

```cpp
auto result =
match(token,
when<token_type::number>(parse_number),
when<token_type::identifier>(parse_identifier),
otherwise(report_error)
);
```
Ideal for:

- interpreters
- lexers
- command dispatch
- protocol handlers

---

### Embedded Language Construction

`DSLUtils.hpphpp` is designed for building complete embedded languages.

Possible applications:

- scripting engines
- shader languages
- rule systems
- symbolic mathematics
- query languages
- parser generators
- reactive systems
- workflow engines

---

## Design Goals

- Modern C++20 APIs
- Strong compile-time guarantees
- Zero-cost abstractions
- Functional composition
- Reusable DSL primitives
- Header-only portability

