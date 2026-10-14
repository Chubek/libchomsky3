# Chapter 22: The `Result<T,E>` Type and Error Flow

DSLtk treats failure as an ordinary value rather than an invisible jump. The `dsl::Result<T,E>` type is the toolkit's canonical carrier for a computation that can either succeed with a value of type `T` or fail with an error of type `E`. It is the explicit-error-flow counterpart to the optional-value machinery of Chapter 20 and the monadic `std::optional` helpers of Chapter 21. Where a `Maybe<T>` can express *that* a computation produced nothing, a `Result<T,E>` can also express *why*.

This chapter documents the public interface of `Result<T,E>` as defined in `DSLtk.hpp`, explains the combinators that operate on it, and shows how to assemble fallible pipelines that propagate the first error. The design ties directly to the first design pillar articulated in Chapter 1: the library prefers visible, inspectable error states over exceptions for control flow. Errors are values, and values compose.

## The Type and Its Storage

`Result<T,E>` is a thin, value-semantic wrapper around a `std::variant<T,E>`. The success branch stores a `T` at alternative index `0`; the error branch stores an `E` at alternative index `1`. The class holds a single private member `data_` of type `std::variant<T,E>`, and every query, accessor, and combinator on `Result` is a disciplined forwarding of `std::variant` access.

Because the storage is a variant rather than a pointer or a sentinel, a `Result` object always holds exactly one of the two payloads. There is no "empty" state distinct from the error state, and there is no additional nullable overhead beyond what `std::variant` itself requires. This makes `Result` cheap to copy, move, and return by value, which is essential because the type is intended to flow through chains of combinators.

```cpp
namespace dsl {
template <typename T, typename E> class Result
{
  // ...
private:
  std::variant<T, E> data_;
};
}
```

The two template parameters are the success type `T` and the error type `E`. The library does not constrain either parameter beyond what `std::variant` requires: both must be object types that are constructible and destructible in the usual ways. Callers are free to choose a small scalar error such as `int`, a descriptive `std::string`, or a dedicated error-code enumeration.

## Construction: `from_ok` and `from_err`

The primary, unambiguous way to build a `Result` is through the static factories. `Result<T,E>::from_ok(v)` constructs a success result carrying `v`, while `Result<T,E>::from_err(e)` constructs an error result carrying `e`. Both factories move their argument into the underlying variant and return a prvalue of type `Result<T,E>`.

```cpp
auto good = dsl::Result<int, std::string>::from_ok (7);
auto bad  = dsl::Result<int, std::string>::from_err (std::string{"overflow"});
```

The factories are the recommended construction path because they name the role of the argument explicitly. A reader of the call site can tell at a glance whether the value is intended as a success or an error, which removes the ambiguity that arises when `T` and `E` happen to share a representation.

In addition to the factories, `Result` provides two implicit converting constructors. The constructor `Result(T value)` places its argument in the success branch, and `Result(E error)` places its argument in the error branch. The error constructor is constrained with a `requires (!std::same_as<T, E>)` clause so that, when `T` and `E` are the same type, the compiler does not have to resolve an ambiguous overload set.

```cpp
dsl::Result<int, std::string> r = 42;          // success branch
dsl::Result<int, std::string> e = std::string{"io"}; // error branch
```

When `T` and `E` are distinct, both constructors participate in overload resolution, and a value of either type converts silently. When `T` and `E` are identical, only the success constructor remains, and the error branch must be reached through `from_err` or through the private in-place constructor that `from_err` invokes internally. This design keeps the common case ergonomic while preventing ambiguity in the degenerate case.

## The `Ok` and `Err` Wrappers

DSLtk also provides two free function templates, `dsl::Ok` and `dsl::Err`, which package a value into a small wrapper struct. `Ok(t)` returns an `OkWrap<T>` whose single member `value` holds `t`; `Err(e)` returns an `ErrWrap<E>` whose single member `error` holds `e`. The wrappers deduce their template argument from the function argument, so the caller does not write the type.

```cpp
template <typename T> struct OkWrap  { T value; };
template <typename E> struct ErrWrap { E error;  };

template <typename T> OkWrap<T>  Ok  (T value);
template <typename E> ErrWrap<E> Err (E error);
```

These wrappers serve as role-marking tokens. They allow a call site to write `dsl::Ok(42)` or `dsl::Err(std::string{"bad"})` and let the surrounding context determine the full `Result<T,E>` type. They are most useful in generic code and in assignments where the target type already fixes `T` and `E`.

```cpp
auto wrap = dsl::Ok (42);        // OkWrap<int>
auto erp  = dsl::Err (std::string{"oops"}); // ErrWrap<std::string>
```

For direct construction of a `Result` whose full type is already known, the `from_ok` and `from_err` factories are the clearer choice. The wrappers and the factories cover the same conceptual ground; the difference is whether the value or the surrounding type annotation carries the role information.

## Querying State: `is_ok` and `is_err`

Every `Result` exposes two boolean queries. `is_ok()` returns `true` when the success branch is active, and `is_err()` returns `true` when the error branch is active. The two queries are exact complements: exactly one of them is `true` at any time, because the underlying variant always holds exactly one alternative.

```cpp
bool is_ok ()  const { return std::holds_alternative<T> (data_); }
bool is_err () const { return std::holds_alternative<E> (data_); }
```

These queries are the preferred way to branch on a `Result` without extracting a value. A caller that only needs to decide whether to proceed can test `is_ok()` and avoid paying for, or risking, a value extraction. The pattern is direct and reads like a tagged-union test, which is precisely what `Result` is.

```cpp
dsl::Result<int, std::string> r = compute ();
if (r.is_ok ())
  proceed ();
else
  log (/* error access */);
```

Because the queries are `const` and trivially computed from `std::holds_alternative`, they are suitable for use in tight loops and in conditional expressions without performance concern. There is no allocation, no locking, and no exception machinery involved in asking which branch is active.

## Extracting Values: `unwrap` and `unwrap_or`

`Result` offers two value extractors. `unwrap()` returns the success value and throws `std::runtime_error` if called on an error result. `unwrap_or(fallback)` returns the success value if present, otherwise returns the supplied fallback. Together they cover the two common extraction policies: assert-and-fail, and degrade-gracefully.

```cpp
T unwrap () const
{
  if (!is_ok ())
    throw std::runtime_error ("dsl::Result::unwrap called on Err");
  return std::get<T> (data_);
}

T unwrap_or (T fallback) const
{
  return is_ok () ? std::get<T> (data_) : std::move (fallback);
}
```

`unwrap()` is appropriate at a point where the program logic guarantees that the result is a success, or where reaching the error branch is genuinely a programming error. The thrown exception carries a fixed diagnostic string; callers that need richer error information should inspect the error branch directly rather than relying on `unwrap`.

`unwrap_or()` is the safer extractor for code that can meaningfully continue with a default. It never throws, and it moves the fallback into the return value when the error branch is active. Because the fallback has type `T`, the call site must commit to a concrete default, which makes the degradation explicit and reviewable.

```cpp
int n = parse_int (s).unwrap_or (0);
```

Neither extractor exposes the error payload. To read the error value, a caller tests `is_err()` and uses the `std::get<E>` path indirectly, through the combinators described below, or by constructing a parallel accessor. The deliberate asymmetry keeps the surface small: value extraction is a success-oriented operation, and error inspection is handled through the combinators.

## Transforming Success: `map`

The `map` combinator applies a function to the success value, leaving the error branch untouched. Given a callable `fn` that accepts a `T` and returns a `U`, `map(fn)` returns a `Result<U,E>`: a success result is rebuilt from `fn(value)`, while an error result is rebuilt from the same error. The error type `E` is preserved across `map`.

The implementation is a faithful two-branch dispatch over the underlying variant.

```cpp
template <typename Fn>
auto
map (Fn &&fn) const -> Result<decltype (fn (std::declval<T> ())), E>
{
  using U = decltype (fn (std::declval<T> ()));
  if (is_ok ())
    return Result<U, E>{ fn (std::get<T> (data_)) };
  return Result<U, E>::from_err (std::get<E> (data_));
}
```

The return type is deduced from the callable, so `map` adapts to whatever transformation the caller supplies. A `Result<int,E>` mapped with `[](int x){ return std::to_string(x); }` becomes a `Result<std::string,E>`. The error type is carried forward unchanged, which is what makes `map` composable along a chain of pure transformations.

```cpp
auto r = dsl::Result<int, std::string>::from_ok (7)
             .map ([] (int x) { return x + 1; })
             .map ([] (int x) { return std::to_string (x); });
// r : Result<std::string, std::string>
```

`map` is the right combinator when the transformation itself cannot fail. If the transformation can fail, the caller should reach for `and_then` instead. Using `map` with a function that returns a `Result` produces a `Result<Result<...>,E>`, which is almost never what is intended; the pitfall is documented below.

## Transforming Error: `map_err`

`map_err` is the dual of `map`: it transforms the error payload while leaving the success value untouched. Given a callable `fn` that accepts an `E` and returns an `E2`, `map_err(fn)` returns a `Result<T,E2>`. A success result is rebuilt from the same value, and an error result is rebuilt from `fn(error)`.

```cpp
template <typename Fn>
auto
map_err (Fn &&fn) const -> Result<T, decltype (fn (std::declval<E> ()))>
{
  using E2 = decltype (fn (std::declval<E> ()));
  if (is_ok ())
    return Result<T, E2>::from_ok (std::get<T> (data_));
  return Result<T, E2>::from_err (fn (std::get<E> (data_)));
}
```

`map_err` is the primary mechanism for widening or normalizing an error type along a chain. A low-level routine that reports a numeric code can be lifted into a higher-level routine that reports a structured error code, without disturbing the success path. The success type `T` is preserved, which keeps the chain's value type stable while the error type is reshaped.

```cpp
enum class LowErr { overflow, underflow };
struct HighErr { LowErr code; std::string context; };

auto lift = [] (LowErr c) { return HighErr{ c, "math" }; };

auto r = dsl::Result<int, LowErr>::from_err (LowErr::overflow)
             .map_err (lift);
// r : Result<int, HighErr>
```

Because `map_err` produces a fresh `Result<T,E2>` even on the success branch, it can be used freely at the boundary between two layers that disagree on the error type. The caller does not need to branch manually; the combinator handles both cases.

## Chaining Fallible Computations: `and_then`

`and_then` is the flat-map combinator for `Result`. Its callback receives the success value `T` and returns a fresh `Result<U,E>`; the combinator flattens the nesting so that the result is a `Result<U,E>` rather than a `Result<Result<U,E>,E>`. When the input is an error, `and_then` short-circuits: the callback is not invoked, and the error is propagated directly.

```cpp
template <typename Fn>
auto
and_then (Fn &&fn) const -> decltype (fn (std::declval<T> ()))
{
  using R = decltype (fn (std::declval<T> ()));
  if (is_ok ())
    return fn (std::get<T> (data_));
  return R::from_err (std::get<E> (data_));
}
```

The short-circuit behaviour is what makes `and_then` the building block of fallible pipelines. Each step receives a success value only if every preceding step succeeded, and the first error flows unchanged to the end of the chain. The caller never writes an explicit `if` to test for failure between steps.

```cpp
dsl::Result<int, std::string>
parse_int (const std::string &s);

dsl::Result<int, std::string>
validate_positive (int x)
{
  return x >= 0 ? dsl::Result<int, std::string>::from_ok (x)
                : dsl::Result<int, std::string>::from_err ("negative");
}

auto r = parse_int (s).and_then (validate_positive);
// r : Result<int, std::string>
```

The error type `E` is fixed across an `and_then` chain. Each callback must return a `Result<U,E>` with the same `E` as its input, or the chain will not type-check. When a step's natural error type differs, `map_err` is used at that step to widen it into the chain's common error type before continuing.

```cpp
auto r = step_a (x)
             .and_then ([] (A a) {
               return step_b (a).map_err (to_chain_error);
             })
             .and_then (step_c);
```

`and_then` returns the exact type produced by its callback, deduced through `decltype`. This means the success type can change freely along the chain while the error type must agree. The combinator is, in effect, the monadic bind of `Result`, and it is the tool that turns a sequence of fallible steps into a single expression.

## Recovery and Fallbacks

Not every error is fatal. DSLtk offers two complementary ways to recover from an error result. The first is `unwrap_or`, which collapses a `Result<T,E>` to a `T` by substituting a fallback when the error branch is active. This is the simplest form of recovery: the caller abandons the distinction between success and error and commits to a concrete value.

```cpp
int port = parse_port (cfg).unwrap_or (80);
```

The second form of recovery is structural rather than collapsing. Because `Result` exposes `is_err()` and the `from_ok`/`from_err` factories, a caller can inspect the error branch and substitute a fresh success result, in effect rebuilding the `Result` from a recovery value. This is the pattern to use when recovery should produce a new success rather than a bare value, so that the result can continue to flow through further combinators.

```cpp
auto recover = [] (dsl::Result<int, std::string> r)
    -> dsl::Result<int, std::string> {
  if (r.is_err ())
    return dsl::Result<int, std::string>::from_ok (0); // default
  return r;
};
```

For value-level fallbacks, `unwrap_or` is preferable because it is a single expression with no branching. For pipeline-level recovery, where the result must keep its `Result<T,E>` shape so that downstream combinators still apply, the inspect-and-rebuild pattern is the appropriate tool. Both patterns keep the recovery visible at the call site rather than hiding it in an exception handler.

## Building a Fallible Pipeline

The combinators compose into pipelines that read left to right. A typical pipeline begins with a fallible producer, threads the value through one or more `and_then` steps that can each fail, applies pure transformations with `map`, and widens the error type with `map_err` where layers meet. The pipeline's overall type is the success type of the last step paired with the common error type.

```cpp
dsl::Result<int, std::string>
parse_count (const std::string &s)
{
  if (s.empty ())
    return dsl::Result<int, std::string>::from_err ("empty");
  // ... assume success
  return dsl::Result<int, std::string>::from_ok (std::stoi (s));
}

dsl::Result<std::vector<int>, std::string>
build_range (int count)
{
  if (count > 1024)
    return dsl::Result<std::vector<int>, std::string>::from_err ("too large");
  std::vector<int> v (count);
  std::iota (v.begin (), v.end (), 0);
  return dsl::Result<std::vector<int>, std::string>::from_ok (std::move (v));
}

auto out = parse_count (s)
               .and_then (build_range)
               .map ([] (std::vector<int> v) { return v.size (); });
// out : Result<std::size_t, std::string>
```

The pipeline reads as a sequence of intentions: parse, then build, then measure. Each `and_then` step is a potential failure point, and the first error short-circuits the rest. A reader does not have to trace nested conditionals to find where the chain stops; the structure of the expression makes the order and the failure points explicit.

Because the error type is uniform across the chain, the final result carries a single `std::string` error regardless of which step produced it. When steps would naturally produce different error types, a `map_err` is inserted at the boundary to lift each step's error into the common type. This keeps the chain type-correct without forcing every step to share a single error representation.

## Result and the Parser Domain

`Result` is the value type that underlies several higher-level facilities in the toolkit. The parser combinators of Chapter 23 and the diagnostic machinery of Chapter 25 use a richer `ExpectedResult` type whose design mirrors `Result`'s two-branch shape. The familiarity gained here transfers directly: the same `is_ok`/`is_err` discipline and the same `map`/`and_then` reasoning apply, with the error payload growing to carry parser-specific diagnostics.

A small fallible parser can be modelled directly with `Result<T,E>` before the full parser infrastructure is introduced. Consider a function that parses a single integer literal and returns either an AST node or a parse error. The signature makes the failure mode part of the contract.

```cpp
struct Ast { int value; };
struct ParseError { std::string message; std::size_t position; };

dsl::Result<Ast, ParseError>
parse_int_literal (std::string_view text, std::size_t pos)
{
  if (text.empty () || !std::isdigit (text.front ()))
    return dsl::Result<Ast, ParseError>::from_err ({ "not a digit", pos });
  int v = text.front () - '0';
  return dsl::Result<Ast, ParseError>::from_ok (Ast{ v });
}
```

The same routine can be lifted into a larger parse by composing it with `and_then`. Each step that consumes input returns a `Result` pairing the consumed value with the remaining input, and the chain threads the remainder forward. This is, in miniature, the structure that Chapter 23 generalizes.

```cpp
using Step = dsl::Result<std::pair<Ast, std::size_t>, ParseError>;

Step parse_term (std::string_view text, std::size_t pos)
{
  return parse_int_literal (text, pos)
      .and_then ([&] (Ast a) -> Step {
        return Step::from_ok ({ a, pos + 1 });
      });
}
```

The example illustrates why `Result` is the toolkit's preferred explicit-error type: it composes without macros, without exceptions, and without hidden control flow, and it leaves the failure reason in the type signature for every caller to see.

## Result Versus Exceptions

Exceptions and `Result` solve the same problem with opposite emphases. An exception propagates implicitly through the call stack until a handler catches it; the intermediate frames need not mention the failure mode at all. A `Result` propagates explicitly through return values; every function that can fail says so in its signature, and every caller must acknowledge the result at the point of call.

The explicit style has a cost: call sites are decorated with `is_ok` tests or combinator calls. It also has a benefit: the set of values a function can return is exactly the set its signature advertises, and there are no hidden jumps for a reader to overlook. For a toolkit whose design pillar is visible, inspectable error states, the trade-off is deliberate.

`Result`'s `unwrap()` is the one place where the type crosses into exception territory. It throws `std::runtime_error` when called on an error result, and it should be treated as a debug assertion rather than a recovery mechanism. Production code that needs to handle failure should use `unwrap_or`, the combinators, or an explicit `is_err` branch instead of `unwrap`.

## Result Versus Maybe and std::optional

`Maybe<T>`, documented in Chapter 20, and the `std::optional` helpers of Chapter 21 express absence without a reason. A `Maybe<T>` is either a `T` or nothing; the caller learns that a computation failed but not why. `Result<T,E>` generalizes this by attaching an `E` to the failure branch. When the reason matters, `Result` is the right tool; when only presence matters, `Maybe` is sufficient and lighter.

The relationship is mechanical as well as conceptual. A `Result<T,E>` can be reduced to a `Maybe<T>` by discarding the error, and a `Maybe<T>` can be lifted into a `Result<T,E>` by supplying a default error. The combinators mirror each other: `Maybe::map` corresponds to `Result::map`, and `Maybe::or_else` corresponds to `Result`'s recovery patterns. Readers familiar with Chapter 20 will find the combinator discipline identical in spirit.

The distinction is not merely cosmetic. A parser that returns `Maybe<Ast>` on failure gives the caller nothing to report; a parser that returns `Result<Ast,ParseError>` gives the caller a message and a position. The cost of carrying the error is one variant alternative, which is usually a worthwhile exchange for the diagnostic value.

## Pitfalls and Practical Notes

The most common mistake is confusing `map` with `and_then`. `map` expects a pure function `T -> U` and yields `Result<U,E>`; `and_then` expects a fallible function `T -> Result<U,E>` and yields `Result<U,E>`. Passing a function that returns a `Result` to `map` produces a `Result<Result<U,E>,E>`, which compiles but is almost always wrong. The compiler will not catch the intent, only the types, so the choice between the two combinators must be made deliberately.

The second pitfall is error-type drift along a chain. An `and_then` chain requires every step to return the same error type `E`. When a step's natural error type differs, the caller must insert a `map_err` to lift it into the chain's common type before the step is composed. Forgetting the lift produces a type error at the `and_then` call, which is helpful but can be cryptic when the chain is long.

A third pitfall concerns extraction. Calling `unwrap()` on an error result throws, and calling `unwrap_or` on an error result discards the error payload. Neither accessor reveals *why* the result is an error. Code that needs the error reason must test `is_err()` and read the error through the combinators or through a direct branch; relying on `unwrap` and catching the exception is not the intended pattern.

A fourth pitfall is the degenerate case `T == E`. Here the implicit error constructor is disabled, and the error branch can only be reached through `from_err`. Callers that construct `Result<int,int>` results should use the factories exclusively to avoid surprising overload resolution. The constraint is documented on the constructor and is visible in the source.

Finally, callers should remember that `Result` is a value type, not a pointer. Copying a `Result` copies its payload, and moving a `Result` moves it. There is no shared state, and there is no need to allocate. Chains of combinators produce a sequence of temporaries that the compiler is free to elide or move, so the style is suitable for performance-sensitive code paths.

## Summary

`dsl::Result<T,E>` is a value-semantic sum type over `std::variant<T,E>` that carries either a success value of type `T` or an error of type `E`. It is constructed through the `from_ok` and `from_err` factories, through the implicit converting constructors when `T` and `E` differ, and through the `Ok` and `Err` role-marking wrappers. Its state is queried with `is_ok` and `is_err`, and its value is extracted with `unwrap` and `unwrap_or`.

The combinator suite provides the standard functional repertoire. `map` transforms a success value and preserves the error; `map_err` transforms an error and preserves the success; `and_then` flattens a fallible step and short-circuits on the first error. Together they build pipelines that propagate errors explicitly, without exceptions and without hidden control flow. The error type must be consistent across an `and_then` chain, and `map_err` is the tool for widening it at layer boundaries.

`Result` is the explicit-error counterpart to the `Maybe` of Chapter 20 and the `std::optional` helpers of Chapter 21, and it is the conceptual foundation for the `ExpectedResult` type used by the parser combinators of Chapter 23 and the diagnostics of Chapter 25. It embodies the design pillar set out in Chapter 1: failure is a value, and values compose.
