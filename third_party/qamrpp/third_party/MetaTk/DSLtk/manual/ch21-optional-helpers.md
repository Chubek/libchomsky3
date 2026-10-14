# Chapter 21: Monadic Helpers for `std::optional`

Chapter 20 introduced `dsl::Maybe<T>`, a thin wrapper over `std::optional<T>` that exposes `map`, `flat_map`, `filter`, and `or_else` as member functions. That wrapper is convenient when a value originates inside DSLtk code, but it is not always the right shape for the value at hand. A great deal of real code already holds a `std::optional<T>` returned by the standard library, by a third-party API, or by a colleague's function, and rewrapping such a value into a `Maybe<T>` just to call one combinator is noisy.

This chapter documents the second entry point into DSLtk's monadic layer: a set of free functions in namespace `dsl` that operate directly on any optional-like object. The four functions are `dsl::map`, `dsl::flat_map`, `dsl::filter`, and `dsl::or_else`. They provide exactly the same combinators as `Maybe<T>`, but without forcing the caller to adopt a new type.

## Motivation: Combinators Without Rewrapping

`std::optional<T>` is the canonical "value or absence" type in modern C++, and it is used pervasively. Standard-library facilities such as `std::find` return an iterator, but many lookups in user code immediately translate that iterator into a `std::optional<reference>` or a `std::optional<T>`. Map accessors, configuration readers, parsers, and database probes all naturally produce `std::optional`. Once such a value exists, the question is what to do with it.

The idiomatic answer in a monadic style is to chain small transformations: map a function over the contained value, flat_map a function that itself may fail, filter against a predicate, and supply a default. Doing this by hand with `if (opt)` ladders quickly turns into nested, indented code that hides the linear shape of the computation. The combinators in this chapter restore that linear shape.

`dsl::Maybe<T>` (Chapter 20) already offers these combinators as members, but only on its own type. Constructing a `Maybe<int>` from an existing `std::optional<int>` is cheap, yet it is still a syntactic speed bump: every chain start becomes `dsl::Maybe<int>{opt}.map(...)` instead of `dsl::map(opt, ...)`. The free-function form removes the bump.

These helpers therefore exist for a single reason: to let monadic composition reach into the existing universe of `std::optional` values without requiring a wrapping conversion. They are the adapter that bridges the standard library's optional type and DSLtk's combinator vocabulary.

## The Four Free Functions

DSLtk exports four free function templates in namespace `dsl`. Each takes an optional-like object as its first argument and a callable as its second, and each returns a value of a type deduced from the callable. The signatures are generic in the optional type, so any type that supports `operator*`, `operator!`, and (for `or_else`) `value_or` will work; in practice this means `std::optional<T>` and `dsl::Maybe<T>` alike.

The four functions correspond one-to-one with the four member functions of `Maybe<T>`. `dsl::map` lifts a plain function into optional space. `dsl::flat_map` (the `and_then` combinator) handles functions that themselves return an optional. `dsl::filter` keeps or discards a value based on a predicate. `dsl::or_else` supplies a fallback when the optional is empty.

Their semantics are identical to the member-function versions documented in Chapter 20. The only difference is the call site: free functions take the optional as an explicit first argument, where member functions take it as `*this`. This makes them composable with raw `std::optional` values produced anywhere in a program.

Because the function templates deduce their return type from the callable, no explicit template arguments are needed at the call site. The compiler infers `U` from `f(*opt)` for `map`, the optional-return type for `flat_map`, the original optional type for `filter`, and the value type for `or_else`. This keeps call sites terse.

## `dsl::map`: Lifting a Function

`dsl::map` applies a function to the contained value when the optional is engaged, and returns an empty optional otherwise. It is the simplest of the four combinators and the most frequently used. Its declaration, quoted verbatim from the header, is:

```cpp
template <typename Opt, typename Fn>
auto
map (const Opt &opt, Fn &&fn) -> std::optional<decltype (fn (*opt))>
{
  if (!opt)
    return std::nullopt;
  return fn (*opt);
}
```

The return type is always a `std::optional<U>`, where `U` is the return type of `fn(*opt)`. This is true even when `U` is itself an optional-like type; `dsl::map` does not flatten nested optionals. That flattening is the job of `flat_map`, described below.

A short example shows the typical shape. Given an engaged optional, the function is applied and the result is wrapped; given an empty optional, the result is empty.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <iostream>

int main() {
  std::optional<int> v = 7;
  auto mapped = dsl::map(v, [](int x) { return x + 1; });      // optional<int>{8}
  std::optional<int> empty;
  auto nothing = dsl::map(empty, [](int x) { return x + 1; }); // nullopt
  std::cout << mapped.value_or(0) << ' ' << nothing.value_or(-1) << '\n';
}
```

`dsl::map` is the right combinator when the transformation cannot fail. A function `int -> double` always produces a `double`, so wrapping its result in an optional merely propagates absence from the input. If the transformation itself can fail, the function should return an optional and `dsl::flat_map` should be used instead.

Note that the callable receives the unwrapped `T`, not the `optional<T>`. Inside `fn`, the value is a plain `int`, `std::string`, or whatever the optional holds. This matches the convention used by `Maybe<T>::map` and by `std::optional::transform` in C++23.

## `dsl::flat_map`: The `and_then` Combinator

`dsl::flat_map` is the monadic bind operator. Its callable returns an optional, and `flat_map` flattens that result: if the input optional is empty, the empty optional propagates directly; otherwise the callable is invoked and its returned optional becomes the result. The verbatim declaration is:

```cpp
template <typename Opt, typename Fn>
auto
flat_map (const Opt &opt, Fn &&fn) -> decltype (fn (*opt))
{
  using R = decltype (fn (*opt));
  if (!opt)
    return R{};
  return fn (*opt);
}
```

The return type is exactly the type returned by `fn`, which must be an optional-like type default-constructible to the empty state. When the input is empty, `flat_map` returns a default-constructed `R{}` rather than invoking `fn`; this short-circuiting is the whole point of the combinator.

`flat_map` is the combinator to reach for whenever a step in a pipeline can itself fail. Parsing a string into an integer may fail; looking up an integer in a table may fail; converting an integer into a valid enum may fail. Each of these steps returns an optional, and chaining them with `flat_map` produces a single optional that is engaged only if every step succeeded.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <string>

static dsl::Maybe<int> to_int(std::string s) {
  if (s.empty()) return {};
  return std::stoi(s);
}
static std::optional<double> reciprocal(int x) {
  if (x == 0) return std::nullopt;
  return 1.0 / x;
}

int main() {
  std::optional<std::string> input = std::string("4");
  auto out = dsl::flat_map(dsl::flat_map(input, to_int), reciprocal);
  return out ? int(*out * 10) : 0;
}
```

The distinction between `map` and `flat_map` is the same as in Chapter 20 and bears repeating. `map` is for total functions `T -> U`; `flat_map` is for partial functions `T -> optional<U>`. Passing a function that returns an optional to `map` yields an `optional<optional<U>>`, which is almost never what is wanted. Passing a function that returns a plain value to `flat_map` is a compile error, because the return type lacks the required default-construction-to-empty semantics.

## `dsl::filter`: Retaining or Discarding

`dsl::filter` keeps the optional's value when a predicate holds, and clears the optional when the predicate fails or when the optional was already empty. Its declaration is:

```cpp
template <typename Opt, typename Pred>
auto
filter (const Opt &opt, Pred &&pred) -> Opt
{
  if (opt && pred (*opt))
    return opt;
  return std::nullopt;
}
```

The return type is the same optional type as the input. An engaged optional whose value satisfies the predicate is returned unchanged; an engaged optional whose value fails the predicate becomes empty; an empty optional stays empty. `filter` never invokes the predicate on an absent value, so the predicate need not handle the empty case.

`filter` is most useful as a guard inside a pipeline. After mapping a value into a more refined form, a filter can assert an invariant: the number is positive, the string is non-empty, the index is in range. If the invariant fails, the pipeline short-circuits to empty.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <iostream>

int main() {
  std::optional<int> n = 7;
  auto odd = dsl::filter(n, [](int x) { return x % 2 == 1; }); // engaged, 7
  std::optional<int> m = 8;
  auto odd2 = dsl::filter(m, [](int x) { return x % 2 == 1; }); // empty
  std::cout << odd.value_or(0) << ' ' << odd2.value_or(0) << '\n';
}
```

Because `filter` returns the same optional type it receives, it composes cleanly with `map` and `flat_map`. A pipeline can interleave transformations and guards without changing the type flowing through the chain.

## `dsl::or_else`: Supplying a Default

`dsl::or_else` collapses an optional into a plain value. If the optional is engaged, it returns the contained value; otherwise it returns the supplied fallback. Its declaration is:

```cpp
template <typename Opt, typename T>
T
or_else (const Opt &opt, T fallback)
{
  return opt.value_or (std::move (fallback));
}
```

The fallback is taken by value and moved into `opt.value_or`. The return type is the value type `T`, deduced from the fallback argument; the caller is responsible for ensuring that `T` is compatible with the optional's contained type. In practice this means passing a fallback of the same type as the optional's value.

`or_else` is the terminator of a monadic chain. Where `map`, `flat_map`, and `filter` keep the computation in optional space, `or_else` exits it, yielding a concrete value that the rest of the program can use without further checking. Placing `or_else` at the end of a pipeline is the conventional way to "unwrap or default".

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <iostream>

int main() {
  std::optional<int> v;
  std::cout << dsl::or_else(v, 42) << '\n';   // 42
  std::optional<int> w = 9;
  std::cout << dsl::or_else(w, 42) << '\n';   // 9
}
```

Because `or_else` returns a value rather than an optional, it cannot be followed by another combinator in the same chain. It is the last step. Intermediate defaults that should remain in optional space can be expressed with `flat_map` plus a small lambda that returns an engaged optional of the fallback.

## Why Free Functions

The decision to expose these combinators as free functions, rather than only as members of `Maybe<T>`, rests on three observations about C++ code in the wild.

First, the standard library owns `std::optional`. Adding member functions to a type the library did not author is impossible without subclassing, and subclassing a standard-library type is a well-known anti-pattern: the standard types have no virtual destructor and are not designed for polymorphic deletion. Free functions extend `std::optional` without inheriting from it.

Second, much existing code already produces `std::optional`. A function that returns `std::optional<Config>` cannot have its callers rewrite every call site to return `Maybe<Config>`. Free functions meet the value where it is, with no conversion required.

Third, free functions compose uniformly across optional-like types. Because the templates are generic in `Opt`, the same `dsl::map` works on `std::optional<T>`, `dsl::Maybe<T>`, and any user-defined optional-like type that supports the required operations. One combinator vocabulary covers a family of types.

The cost is a slightly more verbose call site: `dsl::map(opt, f)` instead of `opt.map(f)`. In practice this is a minor concern, and the gained uniformity across types outweighs it. Chapter 5 discusses the concept-based design philosophy that motivates such generic free-function APIs elsewhere in DSLtk.

## Relationship to `Maybe<T>`

`Maybe<T>` and the free functions are two views of the same combinators. `Maybe<T>::map` calls the same logic that `dsl::map` does; the member version simply passes `*this` as the optional. The two layers exist so that callers can choose the entry point that fits their value.

When a value originates as a `Maybe<T>` — for instance, returned by one of DSLtk's own parsing or lookup helpers — the member-function form is the natural choice. When a value originates as a `std::optional<T>` — for instance, returned by `std::find`-based logic or by a third-party API — the free-function form avoids a wrapping conversion.

The two forms can be mixed in a single pipeline. A `Maybe<T>` produced by DSLtk can be passed to `dsl::filter` as a free function, or a `std::optional<T>` can be wrapped once at the start of a chain and then use member functions thereafter. The choice is stylistic.

It is worth noting that `Maybe<T>` is constructible from a `std::optional<T>`, so wrapping is always available when the member form is preferred. The header doc-comment for the monadic layer shows both forms side by side:

```cpp
// Member form, on Maybe<T>:
auto out = dsl::Maybe<int>(5).map([](int x){ return x + 1; }).or_else(0);

// Free-function form, on std::optional<int>:
std::optional<int> v = 3;
auto out = dsl::map(v, [](int x){ return x * 2; });
```

Either style yields the same result. The chapter's examples use the free-function form throughout, since the chapter's subject is the free-function layer, but the two forms are interchangeable.

## Composition and Pipelines

The combinators are most powerful when chained. Each combinator returns an optional (except `or_else`, which terminates the chain), so the output of one step becomes the input to the next. A pipeline is built by nesting calls or, equivalently, by passing the result of one combinator as the first argument to the next.

Because the free functions take the optional as their first argument, a chain reads naturally inside-out: the innermost call is applied first, and each enclosing call applies the next step. This is the same shape as function composition in the standard library, and it scales to as many steps as needed.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <string>

static std::optional<int> parse_int(std::string s) {
  if (s.empty()) return std::nullopt;
  return std::stoi(s);
}

int main() {
  std::optional<std::string> raw = std::string("42");
  auto step = dsl::map(
                dsl::filter(
                  dsl::flat_map(raw, parse_int),
                  [](int x) { return x > 0; }),
                [](int x) { return x * 2; });
  return step.value_or(0);
}
```

A pipeline like the one above expresses a clear intent: parse, validate, transform. Each step is a small function with a single responsibility, and the combinators handle the absence-propagation between them. There are no `if` ladders and no nested scopes.

When a chain grows beyond three or four steps, the inside-out nesting becomes hard to read. At that point, breaking the chain into named intermediate variables, or wrapping the initial value into a `Maybe<T>` to use the member-function form, often improves clarity. The free functions do not penalize long chains in any way other than readability.

## Worked Example: Parse, Lookup, Transform

Consider a small program that reads a numeric key from a string, looks the key up in a map, and transforms the looked-up value. Each of these three steps can fail: the string may not be a number, the number may not be a key, and the looked-up value may fail a validity check. The combinators express this directly.

```cpp
#include "DSLtk.hpp"
#include <map>
#include <optional>
#include <string>

static std::optional<int> parse(std::string s) {
  if (s.empty()) return std::nullopt;
  return std::stoi(s);
}

static std::optional<std::string> lookup(const std::map<int, std::string>& m, int k) {
  auto it = m.find(k);
  if (it == m.end()) return std::nullopt;
  return it->second;
}

int main() {
  std::map<int, std::string> table{{1, "alpha"}, {2, "beta"}};
  std::optional<std::string> raw = std::string("2");

  auto name = dsl::flat_map(
                dsl::flat_map(raw, parse),
                [&](int k) { return lookup(table, k); });
  return name ? int(name->size()) : 0;
}
```

The two `flat_map` calls correspond to the two steps that can fail. Because `lookup` returns an `optional<string>`, it must be composed with `flat_map`, not `map`; using `map` would yield an `optional<optional<string>>`, which is a type error at the next step. Adding a transformation that cannot fail — say, uppercasing the result — is a job for `map`.

```cpp
auto upper = dsl::map(name, [](std::string s) {
  for (auto& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
  return s;
});
```

And adding a validity check — say, requiring the result to be non-empty — is a job for `filter`.

```cpp
auto non_empty = dsl::filter(upper, [](const std::string& s) { return !s.empty(); });
```

The chain now has four steps, each with a clear role, and absence propagates automatically from any of them to the final result. Terminating with `or_else` yields a concrete string.

```cpp
std::string result = dsl::or_else(non_empty, std::string("(none)"));
```

This example illustrates the central payoff of the monadic style: the happy path reads as a straight line of transformations, and the unhappy paths are handled implicitly by the combinators.

## Composing with Standard Library Returns

Many standard-library APIs yield values that fit naturally into optional pipelines. A map lookup, as shown above, is one example. Others include `std::find` on a range (whose result is an iterator that may be `end()`), configuration accessors that return `std::optional<T>` directly, and any function that uses `std::optional` as a return type to signal "no result".

The free functions accept any optional-like type, so values from these diverse sources can be mixed in a single pipeline. A `std::optional<int>` from a config reader can be `flat_map`ped into a `std::optional<Handle>` from a resource manager, then `map`ped into a `std::optional<Result>` from a computation. The types flow through the chain, and absence at any stage produces an empty final result.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <vector>
#include <algorithm>

int main() {
  std::vector<int> xs{1, 2, 3, 4};
  // Translate a found iterator into an optional index, then transform.
  std::optional<int> idx = [&] {
    auto it = std::find(xs.begin(), xs.end(), 3);
    if (it == xs.end()) return std::optional<int>{};
    return std::optional<int>{int(it - xs.begin())};
  }();
  auto doubled = dsl::map(idx, [](int i) { return i * 2; });
  return doubled.value_or(-1);
}
```

The pattern of "convert a sentinel-returning API into an optional, then chain combinators" is a common one. The free functions make it easy to insert a standard-library value at any point in a pipeline without adapting its type.

When the source value is expensive to copy — a large `std::string`, a `std::vector`, or a move-only type — the value-category of the optional matters. The free functions take the optional by `const&`, so the contained value is not copied when the optional is passed in; only the transformations applied to it produce new values. A pipeline that maps a `std::string` to its length, for instance, copies no strings after the initial optional is constructed.

## Defaulting with `or_else`

Supplying a default is the most common way to exit a monadic pipeline. `dsl::or_else` takes the optional and a fallback value, and returns whichever is present. Because it returns a plain value rather than an optional, it is the natural last step of a chain.

A default can be a sentinel, a computed fallback, or a value read from a secondary source. When the fallback is expensive to compute, care should be taken: `or_else` eagerly takes the fallback by value, so the fallback is constructed at the call site regardless of whether the optional is engaged. For an expensive fallback, it may be preferable to compute it lazily or to check the optional explicitly before constructing it.

```cpp
#include "DSLtk.hpp"
#include <optional>
#include <string>

int main() {
  std::optional<std::string> cfg;
  // Eager fallback: the literal is cheap to construct.
  std::string mode = dsl::or_else(cfg, std::string("default"));
  return int(mode.size());
}
```

For a default that depends on the absence itself — for instance, logging that a default was used — a small `flat_map` followed by `or_else` can express the side effect while keeping the chain shape. The combinators do not prohibit side effects in the callables, though pure functions are preferred where practical.

## Pitfalls

Several pitfalls recur often enough to deserve explicit mention. They are the same pitfalls that apply to `Maybe<T>` in Chapter 20, restated for the free-function form.

The first is the `map` versus `flat_map` confusion. If a step's callable returns an optional, `flat_map` is required; using `map` instead nests the optional one level deeper and produces a type that the next step cannot consume. The compiler will usually catch this, since an `optional<optional<T>>` will not match a callable expecting `T`, but the diagnostic can be obscure. The rule is simple: if the callable returns an optional, use `flat_map`; otherwise use `map`.

The second pitfall is passing a callable that expects an optional rather than the unwrapped value. The combinators invoke the callable with `*opt`, so the callable receives a `T`, not an `optional<T>`. A lambda written as `[](std::optional<int> o){ ... }` will fail to compile when passed to `dsl::map` on a `std::optional<int>`.

The third pitfall concerns value categories and copies. The optional is taken by `const&`, and `*opt` yields a `const T&`. The callable therefore receives a const reference to the contained value. If the callable needs to mutate or move the value, it must accept a copy; the original optional retains its value. For move-only types, this means the value cannot be moved out through the combinators without an explicit copy or a non-const wrapper.

The fourth pitfall is the eager evaluation of the `or_else` fallback, discussed above. The fallback is constructed at the call site before `or_else` decides whether to use it. For cheap fallbacks this is invisible; for expensive ones it is a hidden cost.

The fifth pitfall is mixing the free-function and member-function forms inconsistently within a single pipeline. While the two forms interoperate without error, a chain that switches between `dsl::map(opt, f)` and `opt.map(f)` is harder to read than one that sticks to a single style. Consistency aids readers.

Finally, the combinators are not a substitute for error reporting. An empty optional carries no information about which step failed. When the location of a failure matters, `Result<T, E>` (Chapter 22) is the appropriate type: it propagates an error value rather than mere absence. The optional combinators are best suited to cases where any failure is equivalent — the answer is either present or not — and where the cost of distinguishing failures is not justified.

## Summary

DSLtk provides four free-function combinators — `dsl::map`, `dsl::flat_map`, `dsl::filter`, and `dsl::or_else` — that extend monadic composition to any optional-like type, with `std::optional<T>` as the primary target. They mirror exactly the four member functions of `dsl::Maybe<T>` (Chapter 20), giving users two entry points into the same combinator vocabulary: a wrapper type for values that originate inside DSLtk, and free functions for values that arrive as raw `std::optional` from the standard library or third-party code.

`dsl::map` lifts a total function `T -> U` into optional space. `dsl::flat_map` binds a partial function `T -> optional<U>`, flattening the result and short-circuiting on absence. `dsl::filter` retains a value only when a predicate holds. `dsl::or_else` collapses an optional to a plain value, supplying a fallback when the optional is empty. Together, they let a pipeline of parse-validate-transform steps be written as a straight line, with absence propagated implicitly between stages.

The free functions are generic in the optional type, so they work uniformly on `std::optional`, `Maybe`, and any user-defined optional-like type that supports the required operations. They avoid subclassing the standard type, they meet existing values where they are, and they compose cleanly with the output of standard-library APIs such as map lookups and `std::find`. The same caveats apply as in Chapter 20: choose `flat_map` when the callable returns an optional, remember that the callable receives the unwrapped value, and prefer `Result<T, E>` (Chapter 22) when the location of a failure must be reported rather than merely its presence.
