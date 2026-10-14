# Chapter 6: The Pipeline Feature

## Motivation

Functional composition is the most natural way to describe a transformation that happens in stages. When the stages are written as nested function calls, the reader must parse them from the inside out: `f2(f1(value))` reads "apply `f1`, then `f2`," but the textual order is reversed relative to the temporal order. The Pipeline feature inverts this so that the reading direction matches the data-flow direction.

DSLtk provides the Pipeline feature so that a DSL author can write `value | pipe(f1) | pipe(f2)` and obtain the value `f2(f1(value))`. Each `|` is a single function application. There is no scheduler, no coroutine machinery, no runtime graph of nodes. The mechanism is a single overloaded `operator|` that forwards its left operand into the callable stored on its right.

This chapter describes every component of the feature: the `PipeStage<F>` wrapper, the `pipe()` factory, the free `operator|`, the `Pipeline` feature tag, and its `Mixin::wrap` static identity lift. It then demonstrates type-changing chains, value semantics, `constexpr` evaluation, composition with other features, and the pitfalls that arise from operator precedence, reference captures, and temporary lifetimes.

## Overview of the Mechanism

The header describes the feature in one line: "Adds `operator|` so values can flow through a chain of transformations." The canonical usage is shown in the header comment as `auto result = value | dsl::pipe(f1) | dsl::pipe(f2);` with the gloss that this is equivalent to `f2(f1(value))`. The rest of the chapter unpacks that equivalence.

Three components cooperate. `dsl::pipe(f)` constructs a `PipeStage<F>` object that holds a copy of `f`. The free `operator|(T&&, PipeStage<F>)` applies the held callable to its left operand. The `Pipeline` feature tag supplies `Mixin::wrap`, a static identity function that marks the start of a chain and returns its argument unchanged. Everything else is convention.

Because the `operator|` is left-associative, `value | pipe(f1) | pipe(f2)` parses as `(value | pipe(f1)) | pipe(f2)`. The inner expression yields `f1(value)`, and the outer expression yields `f2(f1(value))`. The associativity of the operator is what makes the textual order match the evaluation order.

The feature is opt-in. A DSL gains pipeline support only by listing `dsl::Pipeline` among the features of its `dsl::DSL<...>` base. Without that, `wrap` is absent and the only way to use `operator|` is to construct `PipeStage` objects directly via `dsl::pipe`. The feature tag exists so that a DSL has a single, uniform entry point named `wrap`.

## The PipeStage Wrapper

A pipeline stage is any callable that accepts one argument and returns a value. To distinguish such a callable from an arbitrary function object in the type system, the library wraps it in `PipeStage<F>`. The wrapper is a trivial struct with a single data member.

The definition, quoted verbatim from the header, is:

```cpp
template <typename F> struct PipeStage
{
  F fn;
  explicit constexpr PipeStage (F f) : fn (std::move (f)) {}
};
```

The template parameter `F` is the callable's type. The member `fn` holds the callable by value. The constructor is `explicit`, which prevents implicit conversions from a bare callable to a `PipeStage`; a caller must either use the `pipe()` factory or construct the wrapper explicitly. The constructor is also `constexpr`, so a stage can be constructed at compile time.

Because `fn` is held by value, the callable is copied or moved into the stage at construction. A lambda with captured state is therefore copied into the stage, and subsequent uses of the stage operate on that copy. This is the intended value semantics: a stage is self-contained and may be reused, moved, or stored without reference to the original callable object.

The `explicit` constructor has a practical consequence. Writing `5 | [](int x){ return x*2; }` does not compile, because the lambda cannot implicitly become a `PipeStage`. The `pipe()` factory exists precisely to make the construction concise without removing the `explicit` guarantee from the wrapper itself. A user who wants a named, reusable stage can write `auto dbl = dsl::pipe([](int x){ return x*2; });` and then apply it many times.

## The pipe() Factory

The factory is a one-line function template that deduces `F` and constructs a `PipeStage<std::decay_t<F>>`. Quoted verbatim:

```cpp
template <typename F>
constexpr auto
pipe (F &&f)
{
  return PipeStage<std::decay_t<F>>{ std::forward<F> (f) };
}
```

The use of `std::decay_t<F>` as the template argument to `PipeStage` strips references and cv-qualifiers from the deduced type, so `pipe` always produces a stage that owns its callable by value. A lambda passed to `pipe` is therefore copied (or moved, if the argument is an rvalue) into the stage, never held by reference. The `std::forward<F>(f)` inside the braced initializer then forwards the argument to the `PipeStage` constructor, preserving the value category for the inner move or copy.

The factory is `constexpr`, so a stage can be built at compile time. Combined with the `constexpr` `operator|`, this allows an entire pipeline to be evaluated in a constant-expression context. This is demonstrated later in the chapter.

A typical call site is `dsl::pipe([](int x){ return x + 1; })`. The lambda's type is a unique, compiler-generated closure type, and `F` is deduced to that closure type. Because each lambda has a distinct type, two textually identical lambdas produce two distinct `PipeStage` types; this rarely matters in practice, but it means that a `std::vector<PipeStage<???>>` cannot be built without erasing the callable type first.

## The Free operator|

The entire runtime mechanism of the Pipeline feature is a single function template. Quoted verbatim:

```cpp
template <typename T, typename F>
constexpr auto
operator| (T &&val, PipeStage<F> stage)
{
  return stage.fn (std::forward<T> (val));
}
```

The left operand is forwarded as `T&&`; the right operand is a `PipeStage<F>` taken by value. The body is a single call: `stage.fn(std::forward<T>(val))`. The stage's callable is invoked with the forwarded value, and the result is returned. Because the return type is `auto`, it is deduced from the callable's return type, which is how type-changing pipelines are supported without any additional machinery.

The right operand is taken by value. This is consistent with the value semantics of `PipeStage`: the stage is small (typically the size of a lambda's capture) and is moved or copied into the operator. Because the operator returns the result of the call rather than a new stage, the stage is consumed by the application and does not survive the expression. A stage stored in a named variable, however, is copied into the operator and the original remains usable.

The left operand is forwarded perfectly. If the piped value is an lvalue, the callable receives an lvalue reference; if the value is an rvalue, the callable receives an rvalue reference. This means a stage that moves from its argument (for example, a stage that converts a `std::string` into a `std::string_view` and discards the original) can do so correctly when the pipeline is fed a temporary.

Because the operator is a free function in the global namespace (with `T` and `F` as unconstrained template parameters), it is found by argument-dependent lookup whenever a `PipeStage<F>` appears as the right operand. The operator is also `constexpr`, which is what makes compile-time pipelines possible.

## The Pipeline Feature Tag

The feature tag `dsl::Pipeline` is the hook through which a DSL acquires the `wrap` static member. Its definition, quoted verbatim:

```cpp
struct Pipeline
{
  template <typename Derived> struct Mixin
  {
    template <typename T>
    static constexpr T
    wrap (T &&v)
    {
      return std::forward<T> (v);
    }
  };
};
```

The `Mixin` is injected into the DSL's CRTP base by the `dsl::DSL<...>` template described in Chapter 3. The `Derived` parameter is the user's DSL type; `wrap` does not use it, but the parameter is present for uniformity with other feature mixins described in Chapter 5.

The `wrap` member is a static, `constexpr`, identity function. It takes a forwarding reference and returns `std::forward<T>(v)`. It does not construct a wrapper, tag the value, or alter its type. Its sole purpose is to provide a uniform entry point that marks the beginning of a pipeline. A call such as `Proc::wrap(10)` reads as "lift the value 10 into the pipeline," even though at the type level nothing has changed.

Because `wrap` returns the value unchanged, the `operator|` overload is the only thing that can follow it in a chain. The expression `Proc::wrap(10) | dsl::pipe(f)` is therefore exactly `f(10)`. The `wrap` call is, in a sense, documentation: it tells the reader that what follows is a pipeline and not an ad-hoc use of `operator|`. A DSL author who omits `wrap` and writes `10 | dsl::pipe(f)` obtains the same result, but loses the visual cue and the uniformity with other DSLs.

The `constexpr`-ness of `wrap` means a chain can begin in a constant-expression context. Combined with the `constexpr` `pipe()` factory and the `constexpr` `operator|`, an entire pipeline can be evaluated at compile time, as shown later.

## Declaring a DSL With the Pipeline Feature

To use the feature, a DSL lists `dsl::Pipeline` among its features. The minimal declaration is:

```cpp
struct Proc : dsl::DSL<Proc, dsl::Pipeline> {};
```

Here `Proc` is the user's DSL type, passed as the first template argument (the `Derived` parameter of the CRTP base). The `dsl::Pipeline` argument causes the `Mixin::wrap` static member to be available on `Proc`. Once declared, `Proc::wrap` can be used to begin a chain.

A DSL commonly combines several features. Chapter 1's `BasicDSL` combines `Pipeline` with `Operators`:

```cpp
struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};
```

The order of feature tags in the parameter pack does not affect the availability of `wrap`; the mixin injection is described in Chapter 5. A DSL with both `Pipeline` and `Operators` can build a pipeline and also compose predicates, as the example below demonstrates.

## A First Worked Example

The header's own example is a complete, compiling program. It mirrors the canonical usage and shows the expected result.

```cpp
#include "DSLtk.hpp"

struct Proc : dsl::DSL<Proc, dsl::Pipeline> {};

int main() {
  auto r = Proc::wrap(10)
           | dsl::pipe([](int x){ return x * 2; })
           | dsl::pipe([](int x){ return x + 1; });
  // r == 21
}
```

The chain begins with `Proc::wrap(10)`, which returns `10`. The first `|` applies the doubling stage, yielding `20`. The second `|` applies the incrementing stage, yielding `21`. The intermediate values are prvalues of type `int`; no temporary objects beyond the `PipeStage` wrappers are materialized.

The textual order of the stages matches the order of application. A reader scanning the code from top to bottom sees the value enter the pipeline, then each transformation in sequence. This is the principal ergonomic benefit of the feature over nested calls.

## Type-Changing Pipelines

Because the `operator|` deduces its return type with `auto`, each stage may return a different type. The pipeline's type evolves as the value flows through the stages. Nothing in the mechanism constrains the stages to a uniform type.

A chain that converts an integer to a floating-point value and then to a string demonstrates this:

```cpp
#include "DSLtk.hpp"
#include <string>
#include <sstream>

struct Proc : dsl::DSL<Proc, dsl::Pipeline> {};

int main() {
  auto s = Proc::wrap(7)
           | dsl::pipe([](int x){ return x * 2; })          // int -> int
           | dsl::pipe([](int x){ return x / 2.0; })        // int -> double
           | dsl::pipe([](double d) {                       // double -> std::string
               std::ostringstream os;
               os << d;
               return os.str();
             });
  // s == "7"
}
```

Each `|` produces a prvalue of a different type. The `PipeStage` wrappers themselves have distinct types (`PipeStage<lambda1>`, `PipeStage<lambda2>`, `PipeStage<lambda3>`), and the `operator|` instantiates separately for each pair of operand types. The compiler generates a distinct call site for each stage, with the appropriate argument and return types.

Type-changing chains are the rule rather than the exception in realistic DSLs. A parsing pipeline might accept raw text, produce a token list, then a syntax tree, then a value. A numeric pipeline might widen from `int` to `double` to a fixed-point type. The Pipeline feature imposes no constraint on these transitions; the only requirement is that each stage's parameter type is compatible with the type produced by the preceding stage.

## Value Semantics and Forwarding

A `PipeStage` holds its callable by value. The `pipe()` factory uses `std::decay_t<F>` to strip references, so even when a stage is constructed from an lvalue callable, the stage owns a copy. This means a stage is self-contained: it can be moved, returned from a function, or stored in a container without concern for the lifetime of the original callable.

The `operator|` takes the stage by value as well. When a stage is passed to the operator, it is copied or moved into the operator's parameter. If the stage was held in a named variable, the original is unaffected (assuming the callable's copy constructor leaves the original usable). If the stage was a temporary, it is moved. This makes pipelines safe to write inline without considering aliasing.

The piped value, by contrast, is forwarded. The `T&&` parameter in `operator|` is a forwarding reference, and `std::forward<T>(val)` preserves the value category. A stage can therefore declare its parameter as a value, an lvalue reference, or an rvalue reference, and the call site will behave accordingly.

A stage that wishes to consume its argument can declare an rvalue-reference parameter:

```cpp
auto drain = dsl::pipe([](std::string&& s) {
  // s is an rvalue; can be moved from
  return s.size();
});
auto n = Proc::wrap(std::string("hello")) | drain;
```

When the piped value is an rvalue (as above), the stage receives an rvalue reference and may move from it. When the piped value is an lvalue, the same stage would receive an lvalue reference; the stage's parameter type must accept that. Mismatched value categories are diagnosed at compile time by the language's usual reference-binding rules.

## Reusable Stages

Because stages have value semantics, a stage can be named and reused. The same stage can be applied to multiple values without recompilation or re-construction:

```cpp
auto square = dsl::pipe([](int x){ return x * x; });
auto a = Proc::wrap(3) | square;  // 9
auto b = Proc::wrap(4) | square;  // 16
```

Each use of `square` copies the `PipeStage` (and its captured callable) into the `operator|`. The copy is cheap for a captureless lambda, whose closure object is empty. For a lambda with non-trivial capture, the cost is the cost of copying the capture; in performance-sensitive code, the stage can be moved rather than copied by naming it as an rvalue at the call site.

A stage can also be composed into a larger stage by capturing it in a new lambda. This is not a special mechanism; it is ordinary lambda capture applied to a `PipeStage` value. The captured stage is copied into the new lambda's closure and invoked when the new lambda is invoked.

## A Numeric Transform Chain

A realistic numeric pipeline applies a sequence of arithmetic and rounding steps. The example below computes the square root of a sum of squares after scaling.

```cpp
auto normalize = dsl::pipe([](int x){ return static_cast<double>(x); })
               | dsl::pipe([](double d){ return d / 255.0; });
```

Here `normalize` is not itself a `PipeStage`; it is the result of `operator|`, which is the return value of the first stage's callable applied to... nothing. In fact this line does not compile, because `operator|` between two `PipeStage` objects is not defined. The Pipeline feature does not compose stages with each other directly; stages are composed only by applying them to a value.

To build a reusable multi-stage transform, the stages must be captured into a single callable:

```cpp
auto normalize = [](int x) {
  return Proc::wrap(x)
         | dsl::pipe([](int v){ return static_cast<double>(v); })
         | dsl::pipe([](double d){ return d / 255.0; });
};
auto y = Proc::wrap(128) | dsl::pipe(normalize);  // 0.501961...
```

The `normalize` lambda wraps an inner pipeline and is itself wrapped as a single `PipeStage`. This pattern is the idiomatic way to name a multi-stage transform for reuse. The inner pipeline is reconstructed each time the lambda is invoked, which is cheap for captureless lambdas but worth noting for hot loops.

## String Processing Chains

String processing benefits from the left-to-right reading order, because transformations are often described as a sequence of filtering and shaping steps. The example below trims whitespace, lowercases, and replaces spaces with underscores.

```cpp
#include "DSLtk.hpp"
#include <algorithm>
#include <cctype>
#include <string>

struct Text : dsl::DSL<Text, dsl::Pipeline> {};

static std::string trim(const std::string& s) {
  auto b = s.begin(), e = s.end();
  while (b != e && std::isspace(static_cast<unsigned char>(*b))) ++b;
  while (e != b && std::isspace(static_cast<unsigned char>(*(e-1)))) --e;
  return std::string(b, e);
}

int main() {
  auto slug = Text::wrap(std::string("  Hello World  "))
    | dsl::pipe(trim)
    | dsl::pipe([](std::string s){
        std::transform(s.begin(), s.end(), s.begin(),
          [](unsigned char c){ return std::tolower(c); });
        return s;
      })
    | dsl::pipe([](std::string s){
        std::replace_if(s.begin(), s.end(),
          [](unsigned char c){ return std::isspace(c); }, '_');
        return s;
      });
  // slug == "hello_world"
}
```

Each stage is a self-contained function that accepts a `std::string` and returns a `std::string`. The pipeline reads top to bottom as a recipe. Because each stage copies the string (or moves it, where the return is a prvalue and the parameter is taken by value), the chain is safe but not zero-allocation; a production version might pass the string by reference to avoid copies. The Pipeline feature does not prescribe the allocation strategy; that is the stage author's concern.

## A CSV-Like Row Transform

A common DSL application is transforming tabular rows. The example below treats a row as a vector of strings, parses the first field as an integer, scales it, and re-emits the row with the scaled value.

```cpp
#include "DSLtk.hpp"
#include <string>
#include <vector>

struct Rows : dsl::DSL<Rows, dsl::Pipeline> {};

int main() {
  using Row = std::vector<std::string>;
  Row input{"3", "alice", "active"};

  auto out = Rows::wrap(input)
    | dsl::pipe([](Row r){
        int n = std::stoi(r[0]);
        r[0] = std::to_string(n * 10);
        return r;
      })
    | dsl::pipe([](Row r){
        r.push_back("processed");
        return r;
      });
  // out == {"30", "alice", "active", "processed"}
}
```

The first stage mutates the row in place and returns it. The second stage appends a marker. Because each stage returns the row by value, the chain does not alias the original `input` vector; `input` is unchanged after the pipeline runs. (The first stage receives a copy because `wrap` forwards the lvalue `input` as an lvalue reference, and the stage takes its parameter by value, which copies.)

A more careful implementation might move the row through the chain to avoid copies. This requires the stages to take their parameter by value and the caller to feed an rvalue:

```cpp
auto out = Rows::wrap(std::move(input)) | dsl::pipe(/* ... */);
```

After `std::move`, `input` is in a valid but unspecified state. The pipeline's stages receive the moved-from vector and operate on it. This is the standard C++ approach to moving a value through a chain of transformations, and the Pipeline feature's perfect forwarding supports it without any special handling.

## An Inspection Stage

Not every stage must transform its input. A logging or inspection stage observes the value and returns it unchanged, allowing the chain to continue. This is a direct consequence of the `operator|` semantics: a stage that returns its argument unchanged is simply a stage whose callable is an identity function with a side effect.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct Proc : dsl::DSL<Proc, dsl::Pipeline> {};

int main() {
  auto log = [](auto x) {
    std::cout << "stage: " << x << '\n';
    return x;
  };
  auto r = Proc::wrap(5)
    | dsl::pipe(log)
    | dsl::pipe([](int x){ return x * 2; })
    | dsl::pipe(log);
  // prints "stage: 5" then "stage: 10"; r == 10
}
```

The `log` stage is a generic lambda, so it accepts whatever type flows into it. This makes it possible to insert the same logging stage at multiple points in a type-changing pipeline. The stage's return type is deduced as the argument's type, so it preserves the type of the flowing value.

Inspection stages are valuable for debugging. They can be inserted temporarily and removed without disturbing the rest of the pipeline. Because the `operator|` is a free function, no special "tap" or "peek" combinator is needed; an identity-with-side-effect lambda is sufficient.

## constexpr Pipelines

The `pipe()` factory, the `PipeStage` constructor, the `operator|`, and `wrap` are all `constexpr`. A pipeline can therefore be evaluated at compile time, producing a value that is then available as a compile-time constant. This is useful for building lookup tables, computing configuration constants, or asserting invariants at compile time.

```cpp
#include "DSLtk.hpp"

struct C : dsl::DSL<C, dsl::Pipeline> {};

constexpr int squared_plus_one(int x) {
  return C::wrap(x)
       | dsl::pipe([](int v){ return v * v; })
       | dsl::pipe([](int v){ return v + 1; });
}

static_assert(squared_plus_one(4) == 17);
```

The `static_assert` verifies the result at compile time. The lambdas are literal-friendly (their captures are empty, so their closure types are literal types), and the `operator|` calls are constant expressions. The entire chain is evaluated by the compiler, and no runtime code is emitted for the pipeline itself.

A `constexpr` pipeline is subject to the usual constraints of constant evaluation: no dynamic allocation, no I/O, no side effects that the constant-evaluator cannot perform. The inspection-stage pattern shown above cannot be used in a `constexpr` context because `std::cout` is not usable in constant expressions. A stage that returns its input unchanged without side effects, however, is fine.

Compile-time pipelines compose naturally with `consteval` functions and with template metaprogramming. A `consteval` function that builds a pipeline and returns its result forces the entire evaluation to occur at compile time, which can be used to precompute tables or to enforce that a configuration value is a compile-time constant.

## Composition With Other Features: Operators

The `BasicDSL` example from Chapter 1 combines `Pipeline` with `Operators`. The pipeline produces a value, and the predicates built with `make_pred` are then applied to that value.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

int main() {
  auto v = BasicDSL::wrap(5) | dsl::pipe([](int x) { return x + 3; })
           | dsl::pipe([](int x) { return x * 2; });
  auto gt5 = BasicDSL::make_pred([](int x) { return x > 5; });
  auto even = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
  std::cout << v << " " << (gt5(v) && even(v)) << "\n";
}
```

The pipeline computes `v == 16`. The predicates, described in Chapter 7, are then combined with ordinary logical operators. The two features do not interfere: `Pipeline` provides `wrap` and the `operator|` for `PipeStage`, while `Operators` provides `make_pred` and the predicate-composition operators described in Chapter 7. A user can also build a stage that applies a predicate and returns a boolean, integrating the two features within a single chain.

## Composition With Other Features: CustomLiterals

The `MiniDSL` example from Chapter 17 combines `Pipeline` with `CustomLiterals`. A literal is parsed, then piped through a transformation.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct MiniDSL : dsl::DSL<MiniDSL, dsl::Pipeline, dsl::CustomLiterals> {
  static constexpr auto literals = dsl::literal_set(
      dsl::lit<"_km">([](long double v) { return v * 1000.0L; }),
      dsl::lit<"_m">([](long double v) { return v; }));
};

int main() {
  auto meters = MiniDSL::parse_literal("2.5_km");
  auto shown = MiniDSL::wrap(meters) | dsl::pipe([](long double x) { return x + 10.0L; });
  std::cout << shown << "\n";
}
```

`parse_literal` returns `2500.0L`. The pipeline then adds `10.0L`, yielding `2510.0L`. The `CustomLiterals` feature (Chapter 17) produces the value, and the `Pipeline` feature transforms it. This pattern—parse, then pipe through transformations—is a common shape for embedded DSLs that interpret domain-specific notation.

The features compose without friction because each feature contributes its own mixin members and its own free operators. `Pipeline` contributes `wrap` and the `PipeStage` operator; `CustomLiterals` contributes `literals` and `parse_literal`. A DSL can list both and use either at will.

## Operator Precedence and Parenthesization

The pipe operator `|` has lower precedence than arithmetic operators but higher precedence than the logical operators `&&` and `||`. In practice, this means that a lambda body that uses `|` for bitwise OR must be parenthesized, and that a pipeline expression combined with a logical predicate must be parenthesized. The `BasicDSL` example parenthesizes `gt5(v) && even(v)` because it is the argument to `<<`, not because of the pipeline, but the same principle applies.

A subtler precedence issue arises when a lambda is passed directly to `pipe`. The expression `dsl::pipe([](int x){ return x; })` parses correctly because the lambda is a primary expression. However, an expression such as `dsl::pipe([](int x){ return x; } | other)` would attempt to apply `operator|` between the lambda and `other` before constructing the `PipeStage`. Because the lambda is not a `PipeStage`, this is ill-formed. When in doubt, parenthesize the lambda or name it first.

The bitwise-OR operator `|` is overloaded for `PipeStage` on the right, but it is also the built-in bitwise OR for integer types. A stage whose return type is an integer and which is followed by another stage is unambiguous, because the right operand of `|` is a `PipeStage`, not an integer. But a stage that returns an integer followed by an integer (without a `PipeStage`) is interpreted as bitwise OR. This is rarely a problem in practice, because a pipeline is a sequence of `| pipe(...)` applications, but it is worth knowing.

## Reference Captures and Lifetime

A lambda stage may capture variables by reference. When it does, the stage holds references to the captured variables, and the pipeline's correctness depends on those variables outliving the stage's use. Because stages are typically constructed inline and consumed within the same expression, this is usually safe. But a stage stored in a variable and used later, or a stage used in a pipeline that outlives the enclosing scope, can dangle.

```cpp
auto make_stage() {
  int factor = 10;
  return dsl::pipe([&factor](int x){ return x * factor; });
  // factor is destroyed when make_stage returns; the stage dangles
}
```

The stage returned from `make_stage` holds a reference to a destroyed local. Using it is undefined behavior. The fix is to capture by value: `[factor](int x){ return x * factor; }`. The value semantics of `PipeStage` then ensure the factor is owned by the stage.

For non-copyable captures, or for captures that must be shared between stages, a `std::shared_ptr` can be captured by value. The stage then holds a copy of the shared pointer, and the referenced object lives as long as any stage holds it. This is the same discipline that applies to any lambda with reference captures; the Pipeline feature introduces no additional lifetime hazards, but it also provides no additional lifetime safety.

## Lifetime of Temporaries in the Chain

Each intermediate value in a pipeline is a prvalue. The language's temporary materialization rules ensure that each prvalue lives until the end of the full expression that contains it. Because the `operator|` consumes its left operand within the expression, the intermediate values are alive for exactly as long as they are needed.

A stage that returns a reference to its argument (for example, a stage that mutates in place and returns a reference) can extend a temporary's lifetime across one `|`, but the lifetime does not extend beyond the full expression. This is the standard C++ rule; the Pipeline feature does not alter it. In particular, binding the result of a pipeline to a reference extends the lifetime of the final prvalue, but not of any intermediate prvalue.

For pipelines that produce containers, the move semantics of the containers ensure that the final result is constructed efficiently. A chain `wrap(s) | pipe(f1) | pipe(f2)` where each stage returns a `std::string` by value will move the string through the chain if the stages take their parameters by value and return prvalues. No copies are mandated by the Pipeline feature; the copy/move behavior is determined entirely by the stages' signatures and the language's rules.

## A Larger Example: Multi-Stage Validation

A validation pipeline applies a series of checks to a value, aborting on failure. Because the Pipeline feature does not provide short-circuiting, a validation pipeline typically encodes failure as a value (a `Result`, an `optional`, or a sentinel) and lets each stage pass failures through unchanged. Chapter 22 describes the `Result<T,E>` type, which is well suited to this pattern.

A sketch using `std::optional`:

```cpp
#include "DSLtk.hpp"
#include <optional>

struct Valid : dsl::DSL<Valid, dsl::Pipeline> {};

int main() {
  auto check_positive = [](int x) -> std::optional<int> {
    return x > 0 ? std::optional<int>{x} : std::nullopt;
  };
  auto check_even = [](std::optional<int> o) -> std::optional<int> {
    if (!o) return std::nullopt;
    return *o % 2 == 0 ? o : std::nullopt;
  };
  auto r = Valid::wrap(4)
    | dsl::pipe(check_positive)
    | dsl::pipe(check_even);
  // r has value 4
}
```

Each stage accepts an `std::optional<int>` and returns one. A `nullopt` flows through the remaining stages unchanged. The pipeline does not short-circuit—it executes every stage—but the result is the same as if it did, because the failure-propagating stages are cheap. Chapter 21 describes monadic helpers for `std::optional` that can make this pattern more concise.

This pattern illustrates a general principle: the Pipeline feature provides control flow (sequential application) but not branching. Branching is encoded in the data, not in the pipeline. A stage that needs to choose between two transformations must do so internally and return the appropriate value.

## Composing Stages Into Named Transforms

A long pipeline can be broken into named transforms by wrapping sub-chains in lambdas, as shown earlier. This improves readability and allows reuse. The pattern is:

```cpp
auto parse = [](std::string s) {
  return Proc::wrap(s)
    | dsl::pipe([](std::string t){ /* ... */ return t; })
    | dsl::pipe([](std::string t){ /* ... */ return t; });
};
auto emit = [](std::string s) {
  return Proc::wrap(s)
    | dsl::pipe([](std::string t){ /* ... */ return t; });
};
auto result = Proc::wrap(input)
  | dsl::pipe(parse)
  | dsl::pipe(emit);
```

Each named transform is a single-argument callable, so it can be wrapped in a `PipeStage` and used as one stage of a larger chain. The inner pipelines are reconstructed on each invocation, but for non-hot paths this is negligible. The named transforms can be tested in isolation and reused across pipelines.

This decomposition mirrors the structure of a Unix shell pipeline, where `parse | transform | emit` is expressed as a sequence of named filters. The Pipeline feature brings the same ergonomics to C++ value transformations, with the type system checking the compatibility of each stage's input and output types.

## Pitfalls Summary

The Pipeline feature is small, but a few pitfalls recur. The first is precedence: lambdas combined with `|` must be parenthesized when the lambda is not the sole argument to `pipe`. The second is reference capture: a stage that captures by reference dangles if it outlives the captured variable. The third is lifetime of intermediates: the feature follows C++ rules exactly, so a stage that returns a dangling reference is undefined behavior, just as it would be outside a pipeline.

A fourth pitfall is the assumption that the pipeline short-circuits. It does not. Every stage is executed in sequence, regardless of the values flowing through. A stage that should not execute on a failure value must be written to detect the failure and pass it through. A fifth pitfall is the assumption that `|` between two `PipeStage` objects composes them; it does not, because no such overload exists. Stages are composed by applying them to a value, or by capturing one stage inside another's lambda.

Finally, a pitfall specific to `wrap`: because `wrap` is an identity function, it is easy to forget that it does nothing. A user who writes `Proc::wrap(x)` expecting some transformation to occur will be surprised that `x` is returned unchanged. The purpose of `wrap` is to mark the start of a pipeline and to provide a uniform entry point; the transformation is done by the stages that follow.

## Summary

The Pipeline feature provides left-to-right function composition through a single overloaded `operator|`. The `PipeStage<F>` wrapper holds a callable by value; the `pipe()` factory constructs a wrapper with deduced type; the free `operator|(T&&, PipeStage<F>)` applies the callable to its left operand and returns the result. The `Pipeline` feature tag contributes `Mixin::wrap`, a `constexpr` identity function that marks the start of a chain and returns its argument unchanged.

The mechanism is entirely value-based: stages are moved or copied into the operator, and the piped value is perfectly forwarded. Each stage may return a different type, so type-changing pipelines are first-class. Because the operators are `constexpr`, pipelines can be evaluated at compile time. The feature composes naturally with `Operators` (Chapter 7), `CustomLiterals` (Chapter 17), and the monadic types of Chapters 20 through 22. The principal pitfalls are operator precedence, reference-capture lifetimes, and the absence of short-circuiting, all of which are managed by ordinary C++ discipline.
