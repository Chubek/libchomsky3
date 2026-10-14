# Chapter 20: The `Maybe<T>` Type and Monadic Operations

DSLtk ships a small, self-contained monadic optional called `Maybe<T>`. It lives in the `dsl` namespace alongside the free-function combinators `dsl::map`, `dsl::flat_map`, `dsl::filter`, and `dsl::or_else` that operate on any optional-like value. The type is a thin value wrapper around `std::optional<T>`: it borrows the optional's storage, copy and move semantics, and value-access primitives, then layers fluent combinators on top. The result is a value type that can be chained through pure transformations without the noise of repeated `if` checks.

This chapter describes the `Maybe<T>` API in full: its constructors, accessors, and the four combinators `map`, `flat_map`, `filter`, and `or_else`. It then builds up to realistic pipelines that combine the combinators, and contrasts `Maybe<T>` with the related free functions covered in Chapter 21 and the error-carrying `Result<T,E>` of Chapter 22. The chapter assumes familiarity with `std::optional` and with value-semantic C++20 templates.

## Motivation: Optional Values Without Branching Noise

Optional values are ubiquitous in DSL tooling. A lookup may fail, a parse may produce no token, a rewrite rule may not apply, and a memoized computation (Chapter 18) may not yet have a cached entry. The standard-library answer is `std::optional<T>`, which carries a value or none. The problem is not the representation but the ergonomics: each transformation on an optional demands an explicit check, a dereference, and a re-wrap.

When only one transformation is in play, the boilerplate is tolerable. When several transformations compose, the code degenerates into a staircase of nested `if` statements or temporary variables. The intent of the computation — "apply this, then this, then this, but stop if any step is empty" — is buried under plumbing. `Maybe<T>` exists to make that intent visible.

The combinators on `Maybe<T>` are the same ones functional programmers recognize as functor `map`, monadic `bind` (here called `flat_map`), and the filtering step common to monad libraries. They are not exotic; they are the minimal set needed to express a short-circuiting pipeline of pure functions. By returning a fresh `Maybe` at each step, the combinators compose left to right as a fluent chain.

This approach complements, rather than replaces, the lazy abstractions of Chapter 19. `Lazy<T>` defers a single computation; `Maybe<T>` models a computation that may produce no value. The two compose naturally — a `Maybe<Lazy<T>>` is a deferred computation that may turn out to be unavailable — but they solve different problems and are not interchangeable.

## The `Maybe<T>` Class Template

`Maybe<T>` is a class template with a single type parameter `T`, the contained value type. Its storage is exactly one `std::optional<T>`, exposed through `as_optional()` but otherwise private. Because the storage is an optional, the layout, alignment, and triviality of `Maybe<T>` track those of `std::optional<T>`.

The class is declared in the `dsl` namespace. Its documentation comment states the design intent plainly: it wraps `std::optional` semantics and adds fluent `map`/`flat_map`/`filter`/`or_else`. The comment also notes that the monad laws — left identity, right identity, and associativity — hold when the functions passed to `map` and `flat_map` are pure and side-effect free. This is the standard caveat for any functor or monad: the library enforces types, not purity.

The public surface is deliberately small. There are constructors, three accessors (`has_value`, `as_optional`, and `or_else`), and three templated combinators (`map`, `flat_map`, `filter`). There is no `operator*`, no `operator->`, and no `operator bool`. Access to the contained value is meant to go through the combinators or through `as_optional()` when raw optional access is genuinely required. This keeps the type's vocabulary tight and discourages ad-hoc dereference.

The verbatim declaration shows the shape of the type:

```cpp
template <typename T> class Maybe
{
public:
  /// Constructs empty Maybe.
  Maybe () = default;
  /// Constructs Maybe with a value.
  Maybe (T value) : value_ (std::move (value)) {}
  /// Constructs Maybe from std::optional.
  Maybe (std::optional<T> value) : value_ (std::move (value)) {}
  // ... accessors and combinators ...
private:
  std::optional<T> value_{};
};
```

The defaulted default constructor produces an empty `Maybe`. The value constructor moves (or copies) a `T` into the underlying optional. The `std::optional<T>` constructor lifts an existing optional into the wrapper without copying the optional's value more than necessary. Together these three constructors cover every construction path an application needs.

## Construction

Constructing a `Maybe<T>` is no more complicated than constructing a `std::optional<T>`. The empty form is `Maybe<T>{}`, which value-initializes the underlying optional to `std::nullopt`. The valued form is `Maybe<T>(x)`, where `x` is convertible to `T`. The third form, `Maybe<T>(opt)`, takes a `std::optional<T>` and is useful when interoperating with code that already produces optionals.

```cpp
#include "DSLtk.hpp"
#include <string>

dsl::Maybe<int> empty{};                 // no value
dsl::Maybe<int> valued(42);              // contains 42
dsl::Maybe<std::string> from_opt(std::optional<std::string>{"hi"});
```

The empty form is the identity element for the combinators: any `map`, `flat_map`, or `filter` applied to an empty `Maybe` returns an empty `Maybe` of the appropriate result type. This short-circuit behavior is what makes chaining safe. A pipeline never has to test for emptiness between steps; the combinators propagate it automatically.

Because the constructors take their argument by value and move it into storage, constructing a `Maybe<std::string>` from a string literal goes through one `std::string` construction and one move. There is no surprising copy. For move-only types such as `std::unique_ptr<int>`, the value constructor accepts an rvalue and moves it in; the copy constructor of `Maybe<T>` itself is implicitly deleted when the underlying optional is non-copyable, which is the expected behavior.

The `Maybe(std::optional<T>)` constructor is the bridge to the rest of the standard library. Any function that returns `std::optional<T>` can be absorbed into a `Maybe` pipeline by wrapping its result. This is important in practice because DSLtk's free-function combinators (Chapter 21) operate on raw optionals, and converting between the two representations is a one-liner.

## Value Access

Three members give access to the contained value or its absence. `has_value()` reports whether a value is present. `as_optional()` returns a const reference to the underlying `std::optional<T>`, which is useful when handing the value off to a generic optional consumer. `or_else(fallback)` returns the contained value if present, otherwise the supplied fallback.

The `has_value()` method is `const noexcept` and forwards directly to `std::optional::has_value`. It is the predicate to use when a binary decision must be made at the boundary of a pipeline — for instance, to choose between logging a warning and proceeding with a result.

```cpp
dsl::Maybe<int> m(7);
if (m.has_value()) {
  // proceed
}
```

The `as_optional()` accessor is the escape hatch. It returns `const std::optional<T>&`, exposing the wrapped optional without copying. This is the recommended way to interoperate with code that expects a `std::optional`, including the free-function combinators described in Chapter 21. It is also the way to obtain a reference for `*` and `->` dereference when raw access is unavoidable.

The `or_else` member is the terminating accessor. It takes a fallback of type `T` and returns `T` by value, using `std::optional::value_or` internally. It is the standard way to collapse a `Maybe<T>` back into a plain `T` at the end of a pipeline. The fallback is moved into the return value when the `Maybe` is empty, so passing an rvalue fallback is cheap.

```cpp
int n = dsl::Maybe<int>{}.or_else(-1);   // n == -1
int m = dsl::Maybe<int>(8).or_else(-1);  // m == 8
```

There is no throwing accessor on `Maybe<T>` itself. Code that needs `std::optional`'s throwing `value()` should call `as_optional().value()`. This is a deliberate narrowing of the surface: the combinators are the preferred way to consume a `Maybe`, and a throwing accessor would tempt callers back into the branching style the type is meant to replace.

## The `map` Combinator

`map` is the functor operation: it applies a function to the contained value, if any, and returns a `Maybe` of the result type. If the `Maybe` is empty, `map` returns an empty `Maybe` of the result type without calling the function. The function is therefore never invoked on absence, which is the property that makes `map` safe to chain.

The signature, quoted verbatim from the header, is:

```cpp
/// Applies function to contained value, if present.
/// Applies function returning Maybe-like type, if value is present.
template <typename Fn>
auto
map (Fn &&fn) const -> Maybe<decltype (fn (std::declval<T> ()))>
{
  using U = decltype (fn (*value_));
  if (!value_)
    return Maybe<U>{};
  return Maybe<U>{ fn (*value_) };
}
```

The return type is deduced as `Maybe<decltype(fn(std::declval<T>()))>`, which is `Maybe<U>` where `U` is the result of applying `fn` to a `T`. This means the function passed to `map` must accept `T` (or a value convertible from `T`), not `Maybe<T>`. Passing a function that itself returns a `Maybe` is legal but produces a `Maybe<Maybe<U>>`, which is rarely what is wanted; `flat_map` exists for that case.

The body is straightforward. An empty optional yields an empty `Maybe<U>`. A populated optional yields `Maybe<U>{ fn(*value_) }`. The function is called exactly once, on the dereferenced value, only when a value is present. Note that `map` is a `const` member: it does not mutate the receiver, it produces a fresh `Maybe`.

A simple example increments an integer:

```cpp
auto r = dsl::Maybe<int>(4).map([](int x){ return x + 1; });
// r is Maybe<int>(5)
```

Because the return type is derived from the function, `map` can change the contained type. Mapping a `Maybe<int>` with a function returning `std::string` produces a `Maybe<std::string>`. This is essential for pipelines that transform a value through several types — for instance, parsing an integer from a string and then formatting it back.

```cpp
auto s = dsl::Maybe<int>(42)
           .map([](int x){ return std::to_string(x); })
           .map([](const std::string& s){ return s + "!"; });
// s is Maybe<std::string>("42!")
```

The empty case propagates silently. If the initial `Maybe` is empty, neither lambda is called and the result is an empty `Maybe<std::string>`. This is the short-circuit behavior in its simplest form: a missing input yields a missing output, with no branching in user code.

## The `flat_map` Combinator

`flat_map` is the monadic bind operation. It takes a function that itself returns a `Maybe` (or any type constructible from `std::nullopt`-like emptiness) and flattens the result so that no nesting occurs. Where `map` would produce `Maybe<Maybe<U>>`, `flat_map` produces `Maybe<U>`. This is the combinator for chaining steps that can themselves fail.

The verbatim definition is:

```cpp
template <typename Fn>
auto
flat_map (Fn &&fn) const -> decltype (fn (std::declval<T> ()))
{
  using R = decltype (fn (*value_));
  if (!value_)
    return R{};
  return fn (*value_);
}
```

The return type is `decltype(fn(std::declval<T>()))`, i.e. the exact type returned by `fn`. The body returns a default-constructed `R` (which, for `Maybe<U>`, is an empty `Maybe<U>`) when the input is empty, and otherwise returns `fn(*value_)` directly. Because `fn` returns a `Maybe`, the result is already flat.

The function passed to `flat_map` must accept `T` and return a `Maybe`. This is the difference from `map`: `map`'s function returns a plain value, `flat_map`'s returns a `Maybe`. Choosing the wrong combinator is the most common mistake with this type, and it is covered explicitly in the pitfalls section below.

A typical use is a lookup that may fail. Suppose a function `find_user(int id)` returns `Maybe<User>`. Chaining a second lookup that fetches an email from a `User` requires `flat_map`, because the second lookup also returns a `Maybe`:

```cpp
dsl::Maybe<Email> email =
  find_user(id)
    .flat_map([](const User& u){ return find_email(u); });
```

If `find_user` returns empty, `find_email` is never called and the result is an empty `Maybe<Email>`. If `find_user` returns a user, `find_email` is called and its result — whether valued or empty — becomes the result of the chain. This is exactly the short-circuiting behavior a pipeline needs.

`flat_map` is also the combinator that makes the monad laws hold. Left identity (`flat_map` on a just-constructed value applies the function), right identity (`flat_map` with a function that returns its input unchanged is a no-op), and associativity (the order in which `flat_map`s are nested does not affect the result) all follow from the definition. These laws matter because they let a reader refactor a chain of `flat_map` calls into any parenthesization without changing the meaning.

## The `filter` Combinator

`filter` keeps the contained value only if a predicate holds; otherwise it returns an empty `Maybe`. It does not change the value's type. The predicate is a function from `T` to `bool`. `filter` is the combinator for validation: it inserts a short-circuiting check into a pipeline without breaking the chain.

The verbatim definition is compact:

```cpp
/// Retains value only if predicate passes.
template <typename Pred>
Maybe
filter (Pred &&pred) const
{
  if (value_ && pred (*value_))
    return *this;
  return Maybe{};
}
```

The return type is `Maybe` (i.e. `Maybe<T>`), so the type of the pipeline is unchanged by a filter step. When both a value is present and the predicate accepts it, `filter` returns `*this` — a copy of the current `Maybe`. When either condition fails, it returns a default-constructed (empty) `Maybe`.

A validation example keeps only positive integers:

```cpp
auto r = dsl::Maybe<int>(-3).filter([](int x){ return x > 0; });
// r is empty
auto s = dsl::Maybe<int>(7).filter([](int x){ return x > 0; });
// s is Maybe<int>(7)
```

`filter` composes naturally with `map`. A pipeline can transform a value, then validate it, then transform it again, all in one expression. The first failing filter short-circuits the rest of the chain to empty. This is the pattern for parsing and validating user input, where each step has a precondition.

Because `filter` returns a copy of `*this` on success, the value is copied (or moved, depending on `T`) once per successful filter. For expensive-to-copy `T`, the predicate should be cheap and the number of filter steps small. In practice, filters are usually applied to scalar or reference-semantics values, where the copy is negligible.

## The `or_else` Member

The `or_else` member serves two roles depending on context. As the terminal accessor, it collapses a `Maybe<T>` to a `T` by supplying a fallback. As a recovery step within a pipeline, it can be used after a chain that may have gone empty to substitute a default `Maybe` and continue — though note that the member `or_else` returns a `T`, not a `Maybe`, so pipeline recovery is better expressed by collapsing and re-wrapping, or by using the free-function `dsl::or_else` on the underlying optional (Chapter 21).

The verbatim definition is:

```cpp
/// Returns contained value or fallback.
T
or_else (T fallback) const
{
  return value_.value_or (std::move (fallback));
}
```

The member forwards to `std::optional::value_or`, moving the fallback into the return value when the `Maybe` is empty. Because the return type is `T`, calling `or_else` ends a `Maybe` pipeline: the result is a plain value, not a `Maybe`, and no further combinators apply.

The simplest use is defaulting at the end of a chain:

```cpp
int port = parse_port(config)
             .filter([](int p){ return p > 0 && p < 65536; })
             .or_else(8080);
```

If `parse_port` returns empty or the filter rejects the value, the chain yields `8080`. Otherwise it yields the parsed, validated port. The entire validation-and-default logic is one expression with no branching.

When recovery rather than defaulting is wanted — that is, when the pipeline should continue with an alternative `Maybe` rather than collapse to a value — the idiom is to test `has_value()` and re-wrap, or to convert to the underlying optional and use the free-function `dsl::or_else` from Chapter 21, which returns an optional rather than a value. The member `or_else` is deliberately terminal: it forces a decision, which is usually what the end of a pipeline wants.

## Composition: Building a Pipeline

The combinators compose because each returns a `Maybe`. A pipeline is a single expression in which `map`, `flat_map`, and `filter` calls are chained with the member-call dot. The type of the pipeline may change at each `map` or `flat_map` step; it stays constant through `filter`. The first step that yields an empty `Maybe` short-circuits all subsequent steps to empty.

A canonical example is safe integer parsing. Suppose a function `parse_int(const std::string&)` returns `Maybe<int>`. A pipeline that parses, validates, transforms, and defaults looks like this:

```cpp
int n = parse_int(input)
          .filter([](int x){ return x >= 0; })
          .map([](int x){ return x * 2; })
          .or_else(0);
```

Each step is a pure function. The `filter` rejects negative inputs. The `map` doubles the value. The `or_else` supplies a default of zero. If `parse_int` returns empty, neither the filter nor the map runs, and `or_else` returns zero. If the filter rejects, the map does not run, and `or_else` again returns zero. The chain reads top to bottom as a sequence of intentions.

A lookup chain that changes types illustrates the combination of `map` and `flat_map`. Given a `Maybe<Config>` and lookups that return `Maybe`, the chain fetches a section, then a value within the section, then transforms the value:

```cpp
auto host = load_config()
              .flat_map([](const Config& c){ return c.section("net"); })
              .flat_map([](const Section& s){ return s.value("host"); })
              .map([](const std::string& h){ return normalize_host(h); })
              .or_else(std::string{"localhost"});
```

The two `flat_map` steps each return a `Maybe`, so they are flattened rather than nested. The `map` step transforms a value, not a `Maybe`, so `map` is correct there. The terminal `or_else` collapses the result to a `std::string`.

A validation pipeline that filters and maps in alternation is equally direct. The pattern is to interleave `filter` (for checks) and `map` (for transformations), ending with `or_else` or with `has_value` at a boundary:

```cpp
auto ok = dsl::Maybe<int>(user_input)
            .filter([](int x){ return x >= 1; })
            .filter([](int x){ return x <= 100; })
            .map([](int x){ return x - 1; })
            .filter([](int x){ return x % 2 == 0; });
```

Each filter is an independent precondition. Multiple filters can be chained without introducing temporaries, and the chain short-circuits on the first failure. This is the shape of a parser-validator, and it is the same shape used internally by DSLtk's parser combinators (Chapter 23) when they gate results.

## Value Semantics and Copy/Move

`Maybe<T>` is a value type. Its copy constructor, move constructor, copy assignment, and move assignment are implicitly defined by the compiler and track those of `std::optional<T>`. When `T` is copyable, `Maybe<T>` is copyable; when `T` is move-only, `Maybe<T>` is move-only. There are no reference members, no shared state, and no `virtual` methods.

The combinators are `const` member functions that return a fresh `Maybe` by value. They do not mutate the receiver, so a single `Maybe` can be used as the seed for multiple pipelines without copying concerns beyond the obvious one that each pipeline copies the value as needed. This is the same discipline as arithmetic: `a + b` does not modify `a`.

```cpp
dsl::Maybe<int> seed(10);
auto a = seed.map([](int x){ return x + 1; });
auto b = seed.map([](int x){ return x * 2; });
// seed is still Maybe<int>(10)
```

Move semantics matter for pipelines that carry move-only types. Because `map` constructs its result as `Maybe<U>{ fn(*value_) }`, the function receives a `const T&` (the dereferenced optional) and may move from it only if it takes its parameter by value and the caller passes a movable value. In practice, `map` on a `Maybe<std::unique_ptr<X>>` is uncommon; `flat_map` with a function that takes ownership is the more usual pattern when moves are required. The underlying rule is the same as for `std::optional`: the value is not moved out of a `const` optional.

The defaulted destructor is also implicit. A `Maybe<T>` with a non-trivial `T` destructor will destroy its contained value when the `Maybe` goes out of scope, exactly as `std::optional<T>` would. There are no surprises here, which is the point: `Maybe<T>` is meant to be a drop-in, value-semantic wrapper that adds combinators without adding obligations.

## The Free-Function Combinators

Alongside the member combinators, DSLtk provides free functions `dsl::map`, `dsl::flat_map`, `dsl::filter`, and `dsl::or_else` that operate on any optional-like value, including raw `std::optional`. These are the subject of Chapter 21, but they are mentioned here because they are the natural counterpart to the `Maybe` members and because the two APIs are deliberately parallel.

The free functions are templates over the optional type. Their signatures, quoted verbatim, show the parallel structure:

```cpp
template <typename Opt, typename Fn>
auto
map (const Opt &opt, Fn &&fn) -> std::optional<decltype (fn (*opt))>
{
  if (!opt)
    return std::nullopt;
  return fn (*opt);
}

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

The free `map` returns a `std::optional`, not a `Maybe`. The free `flat_map` returns whatever `fn` returns, default-constructing it on emptiness — the same shape as the member `flat_map`. The free `filter` and `or_else` follow the same pattern. Because `Maybe<T>::as_optional()` returns a `const std::optional<T>&`, a `Maybe` can be handed to the free functions directly when a `std::optional` result is preferred over a `Maybe` result.

The two APIs are interchangeable in intent. The member API is fluent and is the natural choice when the whole pipeline is expressed in `Maybe`. The free API is argument-first and is the natural choice when the input is already a `std::optional` or when generic code must accept any optional-like type. Mixing the two in a single expression is possible by routing through `as_optional()`, though it is usually clearer to pick one style per pipeline.

## Interoperability with `std::optional`

`Maybe<T>` is designed to interoperate with `std::optional<T>` without friction. The `Maybe(std::optional<T>)` constructor lifts an optional into a `Maybe`; the `as_optional()` accessor exposes the wrapped optional by const reference. Converting in either direction is a single call.

```cpp
std::optional<int> opt = 5;
dsl::Maybe<int> m(opt);
const std::optional<int>& back = m.as_optional();
```

This bidirectional bridge matters because the standard library is full of functions that return `std::optional`. Rather than reimplementing those functions to return `Maybe`, a pipeline can wrap their results at the boundary and then use the fluent combinators internally. The boundary is where the representation changes; inside the pipeline, everything is a `Maybe`.

The bridge also makes `Maybe` a good citizen in generic code. A template that accepts any optional-like type can be instantiated with `Maybe<T>` as easily as with `std::optional<T>`, provided it only uses operations common to both (such as `has_value` and dereference). For the free-function combinators, `Maybe` works directly because `as_optional()` returns a reference to a real optional and the free functions only require `operator*` and `operator bool`.

When a `Maybe` must be returned from a function that the standard library expects to produce an `std::optional`, the conversion is equally direct: construct a `Maybe` internally, then return `m.as_optional()` (or rely on the implicit conversion path through the optional constructor). There is no need to choose between the fluent style and standard-library compatibility; both are available at the cost of a single constructor call.

## Worked Example: Safe Integer Parsing

Consider the task of parsing an integer from a string, clamping it to a valid range, and converting it to an enum. Each step can fail: the string may not be a number, the number may be out of range, and the number may not correspond to any enumerator. A `Maybe` pipeline expresses this directly.

```cpp
enum class Level { Low, Medium, High };

dsl::Maybe<int> parse_int(const std::string&);       // returns empty on failure

dsl::Maybe<Level> to_level(int x) {
  switch (x) {
    case 0: return dsl::Maybe<Level>{Level::Low};
    case 1: return dsl::Maybe<Level>{Level::Medium};
    case 2: return dsl::Maybe<Level>{Level::High};
    default: return dsl::Maybe<Level>{};
  }
}

Level resolve(const std::string& input) {
  return parse_int(input)
           .filter([](int x){ return x >= 0 && x <= 2; })
           .flat_map(to_level)
           .or_else(Level::Low);
}
```

The pipeline reads as a specification: parse, validate the range, convert to an enum, default to `Low`. The `flat_map` is necessary because `to_level` itself returns a `Maybe`; the range filter makes the second failure impossible in practice, but the type system still requires `flat_map` because `to_level`'s return type is `Maybe<Level>`, not `Level`.

If the filter were omitted, `flat_map` would still handle an out-of-range input correctly: `to_level` would return an empty `Maybe`, `flat_map` would flatten it to empty, and `or_else` would supply `Level::Low`. The filter is present not for correctness but for clarity — it documents the expected range as an explicit step in the chain. This is a common idiom: filters are used as much for readability as for runtime behavior.

The example also shows the role of `or_else` as a terminator. The pipeline produces a `Maybe<Level>`, and `or_else(Level::Low)` collapses it to a `Level`. After `or_else`, no further `Maybe` combinators are available, which is correct: the function has decided on a value, and the pipeline is over.

## Worked Example: A Lookup Chain

A second example is a multi-level lookup. A configuration store maps names to sections, and each section maps keys to string values. Both lookups may fail. The chain fetches a section, then a value, then trims whitespace.

```cpp
dsl::Maybe<const Section&> find_section(const Store&, const std::string&);
dsl::Maybe<std::string> find_value(const Section&, const std::string&);
std::string trim(const std::string&);

std::string lookup(const Store& store) {
  return find_section(store, "net")
           .flat_map([](const Section& s){ return find_value(s, "host"); })
           .map([](const std::string& v){ return trim(v); })
           .or_else(std::string{"localhost"});
}
```

The two lookups both return `Maybe`, so they are chained with `flat_map`. The `trim` step is a pure transformation, so `map` is correct. The terminal `or_else` supplies a default. If either lookup fails, the chain short-circuits to empty and the default is returned.

This shape scales to any depth. A three-level lookup — store to section, section to key, key to typed value — adds one more `flat_map`. The type of the chain changes at each step, but the structure is uniform: `flat_map` for fallible steps, `map` for infallible transformations, `filter` for checks, `or_else` to terminate.

The example also illustrates the difference between `map` and `flat_map` concretely. Had `trim` been wrapped in a function returning `Maybe<std::string>` (say, to signal that trimming could fail on malformed input), the chain would need `flat_map` for that step too. The choice of combinator is dictated entirely by the return type of the step's function: plain value means `map`, `Maybe` means `flat_map`.

## Worked Example: Validation with `filter` and `map`

A validation pipeline interposes checks between transformations. Suppose an input is read as a string, parsed to an integer, checked for range, scaled, and checked again for a downstream constraint. Each check is a `filter`; each transformation is a `map`.

```cpp
dsl::Maybe<int> read_int(const std::string&);

auto result = read_int(raw)
                .filter([](int x){ return x > 0; })
                .map([](int x){ return x * 10; })
                .filter([](int x){ return x < 1000; })
                .map([](int x){ return x + 1; });
```

The pipeline expresses a sequence of preconditions and transformations. The first failing `filter` short-circuits everything after it. The final `result` is a `Maybe<int>`; the caller can `or_else` it to a default or test `has_value` and report a validation error.

Interleaving `filter` and `map` is the idiomatic way to express multi-step validation. An equivalent implementation with `if` statements would require a temporary variable at each step and a nested branch for each failure. The `Maybe` chain collapses all of that into one expression, with the short-circuit semantics provided by the combinators rather than by hand-written control flow.

A subtle point: the order of `filter` and `map` matters. Filtering before mapping is cheaper when the filter is likely to reject, because the map is skipped. Filtering after mapping is necessary when the filter depends on the mapped value. In the example above, the second `filter` depends on the scaled value, so it must come after the `map`. The pipeline structure makes this ordering explicit and visible.

## Pitfalls: `map` Versus `flat_map`

The most common mistake with `Maybe<T>` is using `map` where `flat_map` is required. If the function passed to `map` returns a `Maybe`, the result is a `Maybe<Maybe<U>>` — a nested optional — rather than the flattened `Maybe<U>` the caller almost certainly wants. The type system does not prevent this, because `Maybe<Maybe<U>>` is a legal type.

```cpp
dsl::Maybe<int> find(int);
auto bad  = dsl::Maybe<int>(1).map(find);       // Maybe<Maybe<int>>
auto good = dsl::Maybe<int>(1).flat_map(find);  // Maybe<int>
```

The `bad` chain is not incorrect — it type-checks and runs — but it is almost never what was intended. A subsequent `map` on `bad` would operate on the outer `Maybe`, passing a `Maybe<int>` to the next function, which is unlikely to match the function's expected `int` parameter. The mistake surfaces as a type error one or two steps downstream, which can be confusing to diagnose.

The rule is simple: if the function returns a `Maybe`, use `flat_map`; if it returns a plain value, use `map`. The function's return type, not its name, determines the combinator. Naming a function `find` does not make it require `flat_map`; returning a `Maybe` does.

A related pitfall is passing a function that takes `Maybe<T>` to `map`. The `map` combinator passes the contained `T` to the function, not the `Maybe<T>`. A function written to accept `Maybe<T>` will either fail to compile (if `Maybe<T>` is not constructible from `T`) or will be called with a `T` and silently misbehave. The function passed to `map` must accept `T` (or a type `T` converts to).

## Pitfalls: Empty Propagation

Empty propagation is the feature, not a bug, but it has a pitfall of its own: it is silent. A `Maybe` pipeline that goes empty produces no output, no error, and no indication of which step failed. For transformations where the absence is the expected negative result, this is ideal. For diagnostics, it is inadequate.

When diagnostics are required, the right tool is `Result<T,E>` (Chapter 22), which carries an error value through the chain in the same way `Maybe` carries emptiness. `Maybe` is for cases where the absence is self-explanatory: a missing lookup, an unmatched pattern, an optional configuration value. `Result` is for cases where the caller needs to know why the step failed.

A second aspect of empty propagation is that `map` and `flat_map` do not call their functions on an empty `Maybe`. This is the property that makes them safe to chain, but it means side effects placed in a `map` lambda will not run if the input is empty. A `map` step that logs the value, for instance, will silently skip the log when the input is absent. Side effects in combinators are best avoided; when they are necessary, they should be placed at the end of the pipeline, after `or_else` or after an explicit `has_value` check.

A third aspect is the interaction between `filter` and emptiness. A `filter` on an empty `Maybe` returns an empty `Maybe` without calling the predicate. This is correct — there is no value to test — but it means a predicate that assumes a value is present will not be exercised on the empty path. Predicates should be pure functions of `T` and should not depend on having been called.

## Pitfalls: Purity and the Monad Laws

The documentation for `Maybe<T>` states that the monad laws hold when the functions passed to `map` and `flat_map` are pure and side-effect free. This caveat is standard but worth elaborating. The laws — left identity, right identity, and associativity — are properties of the algebra, not of the implementation. The implementation upholds them mechanically; impure functions violate them by side effect.

Left identity says that constructing a `Maybe` from a value and then `flat_map`ping a function is equivalent to calling the function directly on the value. Right identity says that `flat_map` with a function that wraps its argument in a `Maybe` is equivalent to the original `Maybe`. Associativity says that the grouping of `flat_map` calls does not affect the result. All three hold for `Maybe` as defined, provided the functions do not mutate shared state or depend on mutable globals.

When a function passed to `map` or `flat_map` has side effects — logging, mutation, I/O — the laws can appear to fail because the side effects occur a different number of times or in a different order than a reader expects. The fix is not to change the combinators but to extract the side effects from the functions. Pure transformations compose; side-effecting procedures do not. This is the same discipline required for any functional pipeline, including the memoized functions of Chapter 18 and the lazy values of Chapter 19.

In practice, DSLtk pipelines that use `Maybe` are almost always pure. The type is small enough that the temptation to embed side effects is low, and the fluent style encourages a separation between transformation (in the chain) and observation (at the chain's end). When this separation is maintained, the monad laws hold and the chain can be refactored with confidence.

## Relationship to `Lazy<T>` and Memoization

`Maybe<T>` is related to, but distinct from, the laziness utilities of Chapters 18 and 19. `Lazy<T>` (Chapter 19) defers a single computation until its result is demanded, then caches it. `Maybe<T>` represents a value that may be absent; it is not deferred and does not cache. A `Maybe<Lazy<T>>` is a deferred computation that may turn out to be unavailable, and a `Lazy<Maybe<T>>` is a computation that, when forced, may yield no value — both are legal and occasionally useful.

Memoization (Chapter 18) caches the result of a pure function keyed by its arguments. A memoized function that returns `Maybe<T>` is the natural way to cache lookups that may fail: the cache stores both presence and absence, so repeated lookups for a missing key do not recompute. The `Maybe` returned by such a memoized function composes directly into the pipelines described in this chapter.

The three utilities are thus complementary. `Lazy` defers, `Maybe` models absence, and memoization caches. A pipeline that fetches a configuration value lazily, validates it with `filter`, transforms it with `map`, and defaults it with `or_else` uses all three. The boundary between them is clean: each utility has a single responsibility, and they connect through the common currency of values.

## Relationship to `Result<T,E>`

`Result<T,E>` (Chapter 22) is the error-carrying counterpart of `Maybe<T>`. Where `Maybe<T>` has two states — present and empty — `Result<T,E>` has two states — ok and err — with the err state carrying an error value of type `E`. The two types share the same combinator vocabulary: `map`, `flat_map` (often called `and_then` on `Result`), `filter`, and `or_else`.

The choice between them is a question of information. If the caller of a function needs to know why it failed, `Result<T,E>` is the right type: the error is carried through the chain and can be inspected at the end. If the caller only needs to know that it failed, `Maybe<T>` is simpler and carries less noise. A lookup in a static map, for instance, typically returns `Maybe`: there is no interesting error to report. A parse, by contrast, often returns `Result`: the position and nature of the parse failure are useful to the caller.

The two types are not interchangeable in a single pipeline without conversion. A `Maybe<T>` can be lifted into a `Result<T,E>` by supplying a default error, and a `Result<T,E>` can be collapsed to a `Maybe<T>` by discarding the error. The conversion is a one-liner in either direction, but it forces a decision: at the point of conversion, information is either added (a default error) or discarded (the error value). Pipelines that mix the two should convert at a well-defined boundary rather than ad hoc.

In DSLtk's parser infrastructure (Chapters 23 through 25), `Result`-like types dominate because parser failures carry diagnostic information. `Maybe` appears in the higher-level orchestration — caching lookups, optional configuration, optional rewrites — where the absence is self-explanatory. The two coexist comfortably, each used where its information content matches the problem.

## Stylistic Notes

A few conventions make `Maybe` pipelines easier to read and maintain. First, prefer one combinator per line, with the dot at the start of the next line, so that the pipeline reads as a vertical list of steps. This mirrors the layout used in the examples above and is the same style used for parser combinators in Chapter 23.

Second, name the step functions when a lambda would be opaque. A lambda like `[](int x){ return x * 2; }` is clear enough, but `[](const Section& s){ return s.value("host"); }` is often better expressed as a named function `host_of`. Named functions document intent, are reusable, and are easier to test in isolation. The pipeline then reads as a composition of named operations, which is closer to a specification.

Third, terminate pipelines explicitly. A pipeline that ends in `or_else` has decided on a value. A pipeline that ends in a `Maybe` has not; the caller must handle the empty case. Ending in `has_value` at a boundary makes the decision point visible. Ending in an un-inspected `Maybe` is a smell, because the emptiness is being propagated without being addressed.

Fourth, keep pipelines pure. Side effects in combinators break the monad laws and make the chain hard to reason about. If a side effect is required — logging, instrumentation — perform it at the end of the pipeline, on a value that has already been collapsed, or in a separate pass. The combinators are for transformation; observation is a separate concern.

Finally, prefer `flat_map` to nested `map` when nesting would occur. A `map` returning a `Maybe` is a sign that `flat_map` was intended. Reading the chain left to right, each step's function should return a plain value for `map` or a `Maybe` for `flat_map`; a chain that mixes the two should do so deliberately, not accidentally.

## Summary

`Maybe<T>` is DSLtk's monadic optional: a thin value wrapper around `std::optional<T>` with fluent `map`, `flat_map`, `filter`, and `or_else` combinators. It is constructed empty as `Maybe<T>{}`, with a value as `Maybe<T>(x)`, or from an existing optional as `Maybe<T>(opt)`. Access is through `has_value()`, `as_optional()`, and the terminal `or_else(fallback)`.

`map` applies a pure function to the contained value, changing the type as the function dictates. `flat_map` applies a function that returns a `Maybe` and flattens the result, which is the combinator for chaining fallible steps. `filter` retains the value only if a predicate holds, providing short-circuiting validation. `or_else` collapses a `Maybe` to a plain value by supplying a fallback.

The combinators compose into pipelines that short-circuit to empty on the first failure, expressing validation, lookup, and transformation logic as a single fluent expression without branching noise. `Maybe<T>` is a value type with the same copy and move semantics as `std::optional<T>`, and it interoperates with `std::optional` through the optional constructor and the `as_optional()` accessor.

The free-function combinators `dsl::map`, `dsl::flat_map`, `dsl::filter`, and `dsl::or_else` (Chapter 21) provide the same operations on raw `std::optional` and on any optional-like type, and are the natural counterpart to the `Maybe` members. For error-carrying flows, `Result<T,E>` (Chapter 22) extends the same combinator vocabulary with an explicit error channel. For deferred computations, `Lazy<T>` (Chapter 19) and the memoization utilities of Chapter 18 complement `Maybe` without overlapping it.
