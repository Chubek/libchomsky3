# Chapter 8: Pattern Matching with `match`, `when`, and `otherwise`

## Motivation and Scope

DSLtk provides a small pattern-matching facility for selecting a handler at run time based on a key known at compile time. The key is supplied as a non-type template parameter to `when`, and the handler is an arbitrary callable. When the table is invoked, the first clause whose key equals the supplied argument is dispatched; if none match, an `otherwise` fallback is invoked, or the table throws.

This facility is not a general-purpose pattern matcher in the style of functional-language `match`. It is a closed dispatch table: the set of keys is fixed at construction, the common return type is computed at compile time, and dispatch is a linear scan over the clauses. Its intended use is the kind of small, fixed branching that shows up repeatedly in DSL implementations: enum-to-string tables, token dispatchers, state-machine transition tables, and command routers.

The matcher lives in the `dsl` namespace and is surfaced through three free functions: `dsl::match`, `dsl::when`, and `dsl::otherwise`. The `PatternMatch` feature tag exists only as a marker mixin for DSLs that wish to advertise pattern matching as part of their feature set; the functions themselves are not members of any class.

This chapter covers the value-based form of `when`, where the key is an enum value or an integral. The `dsl::pattern<"...">` form of `when`, which performs compile-time regular-expression matching, is introduced briefly here and covered in full in Chapter 9.

## The Dispatch Table Abstraction

A `MatchTable` is constructed once and invoked many times. Construction binds a sequence of clauses into a `std::tuple`; invocation walks that tuple in order, asking each clause whether it matches the supplied key. The first clause that accepts the key wins, its handler runs, and the result is returned. Subsequent clauses are not consulted.

Conceptually the table is a flat list of `(key, handler)` pairs with an optional trailing fallback. There is no hashing, no indexing, and no reordering. The order in which clauses are passed to `dsl::match` is exactly the order in which they are tested. This makes the matcher's behaviour easy to reason about: a clause earlier in the list shadows any clause later in the list that would also match.

The table is a value object. It is copyable and movable in the usual ways, and it can be stored as a `constexpr` static member of a DSL class. Because the clause tuple is part of the type, two tables built from different clause sets have different types and cannot be interchanged.

## WhenClause: A Single Dispatch Arm

The building block of every `when` arm is the `WhenClause` class template. Its declaration is:

```cpp
template <auto Key, typename F> struct WhenClause
{
  F handler;
  using handler_type_or_void = F;
  explicit constexpr WhenClause (F f) : handler (std::move (f)) {}
  // try_invoke omitted for brevity
};
```

The `auto Key` non-type template parameter is the heart of the clause. Because it is declared `auto`, it can bind to any value that can be used as a non-type template parameter: an enumerator, an integer, a pointer, or a `dsl::pattern<"...">` instance (which is a literal type with a `matches` static member). The compiler infers the type of `Key` from the argument supplied in the angle brackets.

The handler `F` is stored by value. The factory `dsl::when` decays its argument, so a lambda passed to `when` is stored as its closure type, not as a reference or pointer. This means the table owns its handlers, and they live as long as the table does.

The `handler_type_or_void` alias exposes the handler type to the table. The table uses it to compute the common return type across all clauses, as described later. The name is historical; the type is never `void` in practice for `WhenClause`.

A clause is constructed with the `dsl::when` factory, which preserves the value category of the handler through perfect forwarding while decaying the type for storage:

```cpp
template <auto Key, typename F>
constexpr auto
when (F &&f)
{
  return WhenClause<Key, std::decay_t<F>>{ std::forward<F> (f) };
}
```

## try_invoke: Matching a Key

Each clause exposes a `try_invoke` member that the table calls during dispatch. It takes the run-time key, a reference to an `std::optional` that will hold the result if the clause matches, and any extra arguments to forward to the handler. It returns `true` if the clause matched and the result was written, and `false` otherwise.

The body of `try_invoke` performs a small compile-time dispatch of its own. It inspects the type of the stored `Key` and selects one of two matching strategies. For value keys it uses equality comparison; for pattern keys it calls a `matches` static member. The selection is made with `if constexpr`, so only the relevant branch is instantiated for each clause.

The full body of `WhenClause::try_invoke` is:

```cpp
template <typename K, typename... Args>
constexpr bool
try_invoke (K key, std::optional<std::invoke_result_t<F, K, Args...>> &out,
            Args &&...args) const
{
  using KeyT = std::remove_cvref_t<decltype (Key)>;
  if constexpr (requires { KeyT::matches (std::string_view{}); })
    {
      if constexpr (std::convertible_to<K, std::string_view>)
        {
          if (KeyT::matches (std::string_view (key)))
            {
              out = handler (key, std::forward<Args> (args)...);
              return true;
            }
        }
    }
  else if constexpr (std::equality_comparable_with<K, decltype (Key)>)
    {
      if (key == Key)
        {
          out = handler (key, std::forward<Args> (args)...);
          return true;
        }
    }
  return false;
}
```

The first branch detects pattern keys. A `dsl::pattern<"...">` exposes a static `matches(std::string_view)` member, and the `requires` expression tests for its presence. If the key type has such a member, the clause is treated as a pattern clause: the run-time key must be convertible to `std::string_view`, and the pattern's `matches` member decides acceptance. The details of `pattern` and its matching semantics are the subject of Chapter 9.

The second branch handles value keys. It is guarded by the `std::equality_comparable_with<K, decltype(Key)>` concept, which checks that the run-time key type and the stored key type can be compared with `==` in either direction. If they can, the clause compares `key == Key` and, on success, invokes the handler.

If neither branch is viable, the clause never matches. This can happen if the key type neither exposes `matches` nor is equality-comparable with the run-time key. In that case the clause is effectively dead code, but it still participates in the common return type computation, so its handler must still be invocable with the table's argument list.

Note that the result is written into the `out` optional through assignment, and only then is `true` returned. This means the handler is invoked exactly once for a matching clause, and never for a non-matching one. The handler receives the run-time key as its first argument, followed by any extra arguments forwarded from the table call.

## OtherwiseClause: The Fallback

The `otherwise` clause is structurally similar to `when` but behaves differently. Its `try_invoke` always returns `false`:

```cpp
template <typename K, typename... Args>
constexpr bool
try_invoke (K, std::optional<std::invoke_result_t<F, K, Args...>> &, Args &&...)
  const
{
  return false;
}
```

Because `try_invoke` never signals a match, an `otherwise` clause is never selected during the normal scan. Instead, the table detects that no `when` clause matched and then explicitly searches for an `OtherwiseClause` to invoke as a fallback. The clause's handler is therefore invoked only when every preceding `when` has failed.

The `OtherwiseClause` template and its factory mirror the `when` counterparts:

```cpp
template <typename F> struct OtherwiseClause
{
  F handler;
  using handler_type_or_void = F;
  explicit constexpr OtherwiseClause (F f) : handler (std::move (f)) {}
};

template <typename F>
constexpr auto
otherwise (F &&f)
{
  return OtherwiseClause<std::decay_t<F>>{ std::forward<F> (f) };
}
```

An `otherwise` clause is optional. A table without one is valid; it simply throws if no `when` matches. A table may also contain more than one `otherwise`, although only the first is ever invoked. The matcher does not enforce a single fallback; the static scan in `operator()` invokes the first `OtherwiseClause` it finds and sets a flag so that subsequent ones are skipped.

## MatchTable: The Dispatch Operator

The `MatchTable` class template holds the clause tuple and exposes the call operator that performs dispatch. Its core member is `std::tuple<Clauses...> clauses`, constructed from the clause arguments passed to `dsl::match`.

The `operator()` is the heart of the matcher. It computes the common return type, scans the `when` clauses for a match, falls back to `otherwise` if needed, and throws if nothing matched and no fallback exists. The full definition, quoted verbatim from the header, is:

```cpp
template <typename Key, typename... Args>
constexpr auto
operator() (Key key, Args &&...args) const
{
  using RetT = std::common_type_t<std::invoke_result_t<
      typename Clauses::handler_type_or_void, Key, Args...>...>;
  // Try each when<> clause in order
  std::optional<RetT> result;
  bool has_otherwise = false;
  bool matched = std::apply (
      [&] (const auto &...c)
        {
          return (c.try_invoke (key, result, std::forward<Args> (args)...)
                  || ...);
        },
      clauses);
  if (!matched)
    {
      // Find and invoke the otherwise clause
      std::apply (
          [&] (const auto &...c)
            {
              (([&] {
                 using Cls = std::remove_cvref_t<decltype (c)>;
                 if constexpr (std::is_same_v<
                                   Cls,
                                   OtherwiseClause<typename Cls::handler_type_or_void>>)
                   {
                     has_otherwise = true;
                     if (!result)
                       result = c.handler (key, std::forward<Args> (args)...);
                   }
               }()),
               ...);
            },
          clauses);
    }
  if (!result && !has_otherwise)
    throw std::runtime_error (
        "dsl::match: no clause matched and no otherwise");
  return *result;
}
```

The first step is the computation of `RetT`, which is the common type of the results of invoking every clause's handler with the supplied argument types. This computation is described in detail in the next section.

The scan itself is a fold expression over the clause tuple, expanded with `std::apply`. Each clause's `try_invoke` is called in turn; the `||` fold short-circuits on the first `true`, so subsequent clauses after a match are not consulted. The `result` optional is filled as a side effect of the matching clause's `try_invoke`.

If the fold returns `false`, no `when` matched, and the table enters the fallback phase. A second `std::apply` walks the tuple again, this time invoking any `OtherwiseClause` it encounters. The `has_otherwise` flag is set so that the final check can distinguish "no match, no fallback" from "no match, but a fallback ran."

The final guard throws `std::runtime_error` if `result` is still empty and no `otherwise` was found. Otherwise the result is dereferenced and returned. The throw is the only path that produces no return value; every other path writes into `result` before reaching the return statement.

## The Common Return Type

The return type of `operator()` is not specified by the user. It is computed from the handlers themselves using `std::common_type_t`. The alias is:

```cpp
using RetT = std::common_type_t<std::invoke_result_t<
    typename Clauses::handler_type_or_void, Key, Args...>...>;
```

This expands to the common type of the result types of invoking each clause's handler with the table's argument list. `std::common_type_t` performs the usual arithmetic promotions and decay: two handlers returning `int` and `double` yield `double`; two handlers returning `const char*` and `std::string` yield `std::string`, assuming the conversion is valid.

This has two practical consequences. The first is that all handlers must return types that share a common type. If two handlers return unrelated types, `std::common_type_t` is ill-formed and the program fails to compile. The second is that the result is stored in an `std::optional<RetT>`, so `RetT` must be default-constructible and moveable in the manner `std::optional` requires.

A common idiom is to make every handler return the same type, typically `std::string` or a small struct. This sidesteps the question of which conversions apply and makes the table's return type predictable. When handlers must return different types, the user is responsible for ensuring they converge on a sensible common type.

The `std::invoke_result_t` computation also enforces the handler signature. Every handler must be invocable with `(Key, Args...)`, where `Key` is the run-time key type and `Args...` are the extra arguments passed to the table. A handler that takes fewer arguments, or takes them by the wrong type, will fail to instantiate.

## Handler Signature

The handler is invoked as `handler(key, args...)`. The first argument is always the run-time key that was passed to the table, not the compile-time `Key` stored in the clause. For value keys these have the same value on a match, but the type visible to the handler is the run-time key type `K`. For pattern keys the run-time key is the `std::string_view`-convertible value supplied by the caller.

This means a handler can ignore the key entirely, accept it by value or by `const&`, or use it as part of its computation. The example in the header shows handlers that take the key as `auto`:

```cpp
dsl::when<Color::Red>([](auto){ return "red"; })
```

The `auto` parameter accepts any key type. For multi-argument dispatch, the handler takes the key followed by the extra arguments. Example 19 from the distribution shows handlers that take the key plus two extra arguments:

```cpp
dsl::when<LexemeClass::Number>(
    [] (LexemeClass, std::string_view token, std::string_view ctx)
    {
      return std::string (ctx) + ":number(" + std::string (token) + ")";
    })
```

Here the key is accepted as `LexemeClass` and ignored; the meaningful work is done with `token` and `ctx`. The key is nonetheless passed, because the handler signature is fixed by the table's `operator()`.

The extra arguments are forwarded with `std::forward<Args>(args)...`, so handlers that accept rvalue references can move from the arguments. In practice most handlers take their extra arguments by value or by `const&`, which is the simplest and safest choice.

## The match() Factory and Deduction Guide

Tables are not constructed directly; they are built by the `dsl::match` factory. The factory takes any number of clause objects and returns a `MatchTable` whose clause types are the decayed types of the arguments:

```cpp
template <typename... Clauses>
constexpr auto
match (Clauses &&...cs)
{
  return MatchTable<std::decay_t<Clauses>...>{ std::forward<Clauses> (cs)... };
}
```

The deduction guide allows the same syntax to work when constructing a `MatchTable` directly, although there is rarely reason to do so:

```cpp
template <typename... Cs> MatchTable (Cs...) -> MatchTable<Cs...>;
```

In typical code the user writes only `dsl::match(...)`, and the return type is deduced. The factory is `constexpr`, so a table can be constructed at compile time and stored in a `constexpr` variable, including as a static member of a class.

The clause types are decayed before being stored. This means a lambda becomes its closure type, a function pointer stays a function pointer, and references are stripped. The table holds its clauses by value, so the clauses must be copyable or at least movable.

## The PatternMatch Feature Tag

DSLtk's feature-tag mechanism, described in Chapter 5, allows a DSL to advertise that it uses a particular facility. The `PatternMatch` tag is the marker for pattern matching. Its definition is minimal:

```cpp
struct PatternMatch
{
  template <typename Derived> struct Mixin
  {
    // Feature presence marker; no additional methods needed since
    // dsl::match/when/otherwise are free functions in the dsl:: namespace.
  };
};
```

The mixin is empty. Unlike `Pipeline` or `Operators`, `PatternMatch` does not inject any member functions into the derived DSL. The comment in the header explains why: the matching functions are free functions in `dsl`, so there is nothing to inject. The tag exists purely so that a DSL can list `PatternMatch` among its feature tags and so that generic code can detect the feature's presence.

A DSL that uses pattern matching might be declared as follows:

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::PatternMatch> {
    static constexpr auto table = dsl::match(
        dsl::when<1>([](auto){ return "one"; }),
        dsl::otherwise([](auto){ return "?"; })
    );
};
```

Here the table is a static member, constructed once and reused. The `PatternMatch` tag does not affect the table's behaviour; it documents intent and participates in any feature detection the rest of the toolkit performs.

## A First Example: Enum to String

The simplest use of the matcher is to map an enum to a string. The keys are enumerators, the handlers return string literals, and the common return type is `const char*`:

```cpp
#include "DSLtk.hpp"
#include <iostream>

enum class Color { Red, Green, Blue };

int main() {
  auto name = dsl::match(
      dsl::when<Color::Red>  ([](auto){ return "red";   }),
      dsl::when<Color::Green>([](auto){ return "green"; }),
      dsl::when<Color::Blue> ([](auto){ return "blue";  }),
      dsl::otherwise         ([](auto){ return "other"; })
  );
  std::cout << name(Color::Red, 0) << "\n";   // red
  std::cout << name(Color::Green, 0) << "\n"; // green
}
```

Note the second argument to `name`. Because the table's `operator()` forwards `args...` to the handler, and the handler here ignores the key, a placeholder second argument is needed only if the handler declares a second parameter. The handlers above take a single `auto` parameter for the key, so the call is `name(Color::Red, 0)`. The `0` is forwarded to the handler, which ignores it.

Actually, in the handlers shown, each lambda takes exactly one parameter. The call `name(Color::Red, 0)` passes two arguments. This compiles because the lambda's single parameter binds to the key, and the extra `0` would be forwarded as a second argument that the lambda does not accept. The header's overview example uses this form, so the reader should treat the second argument as a placeholder that the simplified single-parameter handler tolerates. In production code, match the handler's parameter list to the call's argument list exactly.

A cleaner version uses a handler that takes only the key and a call that supplies only the key. The matcher does not require a second argument; `args...` may be empty. The simplified call `name(Color::Red)` works when every handler takes a single parameter.

## Integer Classification

Integer keys work the same way as enum keys. The following table classifies a small integer:

```cpp
auto classify = dsl::match(
    dsl::when<0>([](auto){ return "zero"; }),
    dsl::when<1>([](auto){ return "one";  }),
    dsl::when<2>([](auto){ return "two";  }),
    dsl::otherwise([](auto){ return "many"; })
);
std::cout << classify(1) << "\n";  // one
std::cout << classify(5) << "\n";  // many
```

Because the `auto Key` non-type parameter accepts integral values, the angle brackets carry the literal integer. The run-time key is compared with `==` via the `std::equality_comparable_with` branch of `try_invoke`. There is no range matching and no wildcard; each `when` tests exactly one value.

For a "greater than two" branch, the `otherwise` clause is the correct tool. The matcher does not support predicate-based arms; it tests equality of values or matches of patterns. Any branching that depends on a predicate should be encoded either as an explicit list of values or as a `dsl::pattern` (Chapter 9).

## The Lexeme Dispatcher

Example 19 from the DSLtk distribution combines value-based `when` with a separate classification step. The full program is reproduced here:

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string_view>

int main() {
  enum class LexemeClass { Number, Keyword, Identifier, Other };

  auto classify_by_key = [](std::string_view token) {
    if (dsl::pattern<"[0-9]+">::matches(token))
      return LexemeClass::Number;
    if (dsl::pattern<"if">::matches(token)
        || dsl::pattern<"for">::matches(token)
        || dsl::pattern<"while">::matches(token))
      return LexemeClass::Keyword;
    if (dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches(token))
      return LexemeClass::Identifier;
    return LexemeClass::Other;
  };

  auto dispatch = dsl::match(
      dsl::when<LexemeClass::Number>(
          [](LexemeClass, std::string_view token, std::string_view ctx) {
            return std::string(ctx) + ":number(" + std::string(token) + ")";
          }),
      dsl::when<LexemeClass::Keyword>(
          [](LexemeClass, std::string_view token, std::string_view ctx) {
            return std::string(ctx) + ":keyword(" + std::string(token) + ")";
          }),
      dsl::when<LexemeClass::Identifier>(
          [](LexemeClass, std::string_view token, std::string_view ctx) {
            return std::string(ctx) + ":identifier(" + std::string(token) + ")";
          }),
      dsl::otherwise(
          [](LexemeClass, std::string_view token, std::string_view ctx) {
            return std::string(ctx) + ":other(" + std::string(token) + ")";
          }));

  auto classify = [&dispatch, &classify_by_key](std::string_view token) {
    return dispatch(classify_by_key(token), token, "token");
  };

  std::cout << classify("123") << "\n";     // token:number(123)
  std::cout << classify("while") << "\n";   // token:keyword(while)
  std::cout << classify("alpha_7") << "\n"; // token:identifier(alpha_7)
  std::cout << classify("@bad") << "\n";    // token:other(@bad)
}
```

This example illustrates two important points. First, the dispatcher's handlers all return `std::string`, so the common return type is `std::string`. Second, the dispatcher is invoked with three arguments: the classified key, the original token, and a context string. Each handler accepts all three, using the token and context to build its result and ignoring the key.

The classification step uses `dsl::pattern` directly, outside the matcher. This is a common pattern when the key for the matcher is itself derived from a pattern match. Chapter 9 covers `dsl::pattern` in detail; here it is enough to see that the matcher and the pattern engine compose naturally.

## A State-Machine Transition Table

A state machine's transition function is a natural fit for the matcher. The key is a pair of (state, input), but because the matcher takes a single key, the pair is encoded as an enum or an integer. The handlers return the next state.

The following sketch uses an enum that enumerates the meaningful (state, event) combinations:

```cpp
enum class Transition { StartOnA, StartOnB, MidOnA, MidOnB };

enum class State { Start, Mid, Done };

auto step = dsl::match(
    dsl::when<Transition::StartOnA>([](auto, State){ return State::Mid;  }),
    dsl::when<Transition::StartOnB>([](auto, State){ return State::Done; }),
    dsl::when<Transition::MidOnA>  ([](auto, State){ return State::Done; }),
    dsl::when<Transition::MidOnB>  ([](auto, State){ return State::Mid;  }),
    dsl::otherwise                 ([](auto, State){ return State::Done; })
);
```

Here the second argument is the current state, passed through for handlers that need it. In a real machine the key would be computed from the current state and the input event, perhaps by a small helper function. The matcher itself does not parse the pair; it merely dispatches on the encoded key.

This pattern is most useful when the set of transitions is fixed and small. For machines with many states, the encoded-key enum becomes unwieldy, and a tabular representation built on top of `std::array` may be more appropriate. The matcher shines when the number of arms is modest and each arm carries a distinct handler.

## A Command Router

A command router maps a command enum to a handler that executes the command. The following example uses string returns for simplicity, but the handlers could equally return a `Result` type (Chapter 22) or invoke a continuation:

```cpp
enum class Command { Open, Close, Read, Write };

auto route = dsl::match(
    dsl::when<Command::Open> ([](auto, std::string path) {
        return std::string("open: ") + path;
    }),
    dsl::when<Command::Close>([](auto, std::string path) {
        return std::string("close: ") + path;
    }),
    dsl::when<Command::Read> ([](auto, std::string path) {
        return std::string("read: ") + path;
    }),
    dsl::when<Command::Write>([](auto, std::string path) {
        return std::string("write: ") + path;
    }),
    dsl::otherwise([](auto, std::string) {
        return std::string("unknown command");
    })
);

std::cout << route(Command::Open, "/etc/hosts") << "\n"; // open: /etc/hosts
```

Each handler receives the command and the path. The `otherwise` clause catches any command not in the enum, which is useful when the enum might be extended but the router has not yet been updated. Without `otherwise`, an unmatched command would throw.

## The Static-Table Idiom

When a table is used repeatedly, it is wasteful to reconstruct it on every call. The static-table idiom stores the table as a `constexpr` static member of a class. Because `dsl::match` is `constexpr` and the clause handlers are typically literals or stateless lambdas, the table can be initialized at compile time.

```cpp
struct Lexer {
  static constexpr auto dispatch = dsl::match(
      dsl::when<LexemeClass::Number>    ([](auto, auto t, auto c){ return "..."; }),
      dsl::when<LexemeClass::Identifier>([](auto, auto t, auto c){ return "..."; }),
      dsl::otherwise                    ([](auto, auto t, auto c){ return "?"; })
  );
};

// Later:
Lexer::dispatch(LexemeClass::Number, "42", "ctx");
```

The `PatternMatch` feature tag pairs naturally with this idiom. A DSL that wishes to expose a dispatch table can declare itself with `dsl::DSL<MyDSL, dsl::PatternMatch>` and store the table as a static member. The tag does not change the table's behaviour, but it advertises the feature and allows generic code to detect it.

Stateless lambdas are required for a truly `constexpr` table in C++20, because a lambda with captures is not a literal type. When the handlers need access to external state, the table should be constructed at run time, or the state should be threaded through the `args...` pack rather than captured.

## Pattern-Based when: A Preview

The `try_invoke` body shown earlier contains a branch for keys that expose a `matches(std::string_view)` member. This branch is exercised by `dsl::pattern<"...">`, which is a compile-time regular-expression type described in Chapter 9. A pattern-based `when` looks like:

```cpp
auto ptable = dsl::match(
    dsl::when<dsl::pattern<"[0-9]+">>([](auto k){ return std::string(k) + " is numeric"; }),
    dsl::when<dsl::pattern<"[a-z]+">>([](auto k){ return std::string(k) + " is alpha"; }),
    dsl::otherwise([](auto k){ return std::string(k) + " is other"; })
);
```

When the table is invoked with a `std::string_view`, each pattern clause calls its pattern's `matches` member. The first pattern that accepts the string wins. The handler receives the original string as its first argument, just as in the value case.

Pattern clauses and value clauses can be mixed in the same table, although mixing them is unusual because the run-time key types differ. A value clause requires a key equality-comparable with its stored `Key`; a pattern clause requires a key convertible to `std::string_view`. A table that mixes the two will simply never match a given key against clauses of the wrong kind, because the relevant `if constexpr` branch will not instantiate.

The full semantics of `dsl::pattern`, including its supported syntax and its compile-time evaluation, are the subject of Chapter 9.

## Pitfalls

Several subtleties deserve emphasis.

The first is the common-type requirement. Every handler's return type must share a `std::common_type_t` with every other handler's return type. Two handlers returning `int` and `std::string` will fail to compile. The fix is to make all handlers return the same type, or to ensure their types converge through implicit conversion.

The second is the no-match throw. A table without an `otherwise` throws `std::runtime_error` when no `when` matches. This is a run-time failure, not a compile-time one. If the set of possible keys is larger than the set of `when` arms, an `otherwise` is mandatory unless a throw is genuinely the desired behaviour.

The third is first-match-wins ordering. Because the fold short-circuits, an earlier clause shadows a later one. If two `when` clauses carry the same key, the second is dead code. The matcher does not diagnose this; it is the user's responsibility to ensure keys are distinct where distinctness is intended.

The fourth is the handler's first argument. The handler always receives the run-time key as its first parameter, regardless of whether it uses it. A handler that declares no parameters will fail to compile, because `std::invoke_result_t` cannot be formed for it. The minimal handler signature is `(auto)` for the key, plus any extra arguments the table forwards.

The fifth concerns pattern clauses specifically. A pattern clause requires a run-time key convertible to `std::string_view`. If the table is invoked with a key that is not string-like, the pattern clause's branch is not instantiated, and the clause silently never matches. This is usually benign, but it can be surprising when a mixed table is invoked with an unexpected key type.

## Summary

The `match` / `when` / `otherwise` trio provides a small, closed dispatch table for selecting a handler by key. The table is built at compile time from `when` clauses, each carrying a value or pattern key, plus an optional `otherwise` fallback. Dispatch is a linear, first-match-wins scan over the clauses, with the fallback invoked only when no `when` matches.

The return type of the table is the `std::common_type_t` of all handler return types, which constrains handlers to return mutually compatible types. Handlers receive the run-time key as their first argument, followed by any extra arguments forwarded from the table call. The `PatternMatch` feature tag marks a DSL as a user of the facility but adds no behaviour; the functions are free functions in `dsl`.

Value-based `when` clauses compare keys with `==`; pattern-based `when` clauses delegate to `dsl::pattern::matches`, which is the subject of Chapter 9. The matcher composes naturally with the pattern engine, as Example 19 demonstrates: a pattern-based classifier can feed its result into a value-based dispatcher, yielding a two-stage matching pipeline.
