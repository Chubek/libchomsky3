# Chapter 23: Parser Combinators: `ParsecInput`, `ExpectedResult`, `Parser`

DSLtk ships a small parser-combinator core in the `dsl` namespace, organized around three abstractions: an input cursor (`ParsecInput`), a structured result type (`ExpectedResult<T>`), and a callable model for parsers (`Parser<T>`). This chapter documents those three primitives and the machinery that ties them together. The combinators themselves — sequence, alternative, repetition, optional — are the subject of Chapter 24; this chapter concerns only the substrate on which they are built.

The design is deliberately Parsec-like. A parser is not a function that throws on failure; it is a value, a callable that takes a mutable cursor by reference and returns a result object. Failure is a value, not an exception. This mirrors the philosophy of `Result<T,E>` introduced in Chapter 22, and readers familiar with that type will recognize the shape of the error flow immediately. The parser core reuses the same discipline: errors are carried alongside values, accumulated best-effort, and never raised through the call stack.

This chapter is prerequisite for Chapter 24 (primitive parsers and the basic combinators), Chapter 25 (diagnostics, semantic actions, and the full `run_parser` machinery), and the PEG layers of Chapter 27 and Chapter 28. The token-level integration sketched in `examples/15-tokenize-parse-integration.cpp` also rests on the types documented here.

## The Parser-Combinator Core

The header overview describes the combinator subsystem concisely.

```cpp
 * --- CombinatorParser ---
 *
 *   Functional parser core + combinators (`|`, `&`, `*`, optional()).
 *
 *   Usage:
 *     auto p = dsl::parser([](dsl::ParsecInput&
 * in)->dsl::ExpectedResult<char>{...});
 *
 *   Example:
 *     auto seq = p1 & p2;
```

Three observations follow from this summary. First, parsers are constructed by wrapping a lambda with `dsl::parser`, which deduces the value type from the lambda's return type. Second, a parser's lambda takes `dsl::ParsecInput&` and returns `dsl::ExpectedResult<T>`. Third, parsers compose with operators — `|`, `&`, `*` — into larger parsers; those operators are documented in Chapter 24 and are mentioned here only to motivate why the core types exist.

The core is small enough to read in one sitting. `ParsecInput` is a struct with two fields and four member functions. `ParseError` is a struct with three fields and no member functions. `ExpectedResult<T>` is a struct pairing an `std::optional<T>` with a `ParseError`. `Parser<T>` is a thin wrapper around `std::function`. The entire combinator library is built on these four types plus a handful of free-function templates.

The rest of this chapter walks through each type in turn, in the order a parser author encounters them: first the input cursor, then the error record, then the result type, then the parser model, and finally the `run_parser` entry point.

## ParsecInput: The Input Cursor

`ParsecInput` is the mutable input state passed to every parser invocation. It tracks the source text and a cursor position, and it exposes the primitive operations that low-level parser callables use to inspect and consume input.

```cpp
struct ParsecInput
{
  std::string_view source{};
  std::size_t pos{ 0 };
  char
  peek () const
  {
    return pos < source.size () ? source[pos] : '\0';
  }
  bool
  eof () const
  {
    return pos >= source.size ();
  }
  char
  consume ()
  {
    return eof () ? '\0' : source[pos++];
  }
  std::string_view
  get_span (std::size_t start) const
  {
    return source.substr (start, pos - start);
  }
};
```

The struct is an aggregate with default-initialized members. `source` is a non-owning `string_view`; the caller is responsible for keeping the underlying storage alive for the duration of the parse. `pos` is a `std::size_t` index into `source`, defaulting to zero. There is no separate end pointer; the cursor's extent is always `source.size()`.

Because `ParsecInput` is an aggregate, it can be value-initialized with brace syntax. `dsl::run_parser` constructs one internally as `ParsecInput{ source, 0 }`. User code that needs to drive a parser by hand — for example, to parse a prefix and then continue manually — can construct a `ParsecInput` the same way.

The cursor is passed to parsers by reference. A parser that succeeds typically advances `pos` by one or more positions; a parser that fails is expected to leave `pos` where it found it, or to be wrapped by a combinator that restores it. The combinators in Chapter 24 enforce the save-and-restore discipline; a hand-written parser must respect it manually.

Note that there is no `remaining()` member and no `advance(n)` member. Input is consumed one character at a time through `consume()`, and the remaining suffix is always available as `source.substr(pos)` should a parser need it. The public `pos` member also permits direct index arithmetic, which the combinators use for save/restore.

## Position, Peeking, and End of Input

`peek()` returns the character at the current cursor position without advancing. If the cursor is at end-of-input, it returns `'\0'`. This sentinel is unambiguous for text parsing: a real NUL in the source would be returned exactly the same way, but in practice DSLtk's parsers operate on source text where NUL does not occur. A parser that wishes to distinguish "NUL in input" from "past end" should call `eof()` first.

```cpp
bool at_digit(dsl::ParsecInput &in) {
  return !in.eof() && in.peek() >= '0' && in.peek() <= '9';
}
```

`eof()` returns `true` when `pos >= source.size()`. It is the recommended guard before any `peek()`-based decision where the distinction between end-of-input and a real `'\0'` matters. The built-in `satisfy` parser, documented below, follows exactly this pattern.

`pos` is a public member, so a parser may inspect it directly to record where a token began. The `get_span` member uses two positions to extract a substring of the source, which is how token-style parsers recover lexeme text after consuming characters.

The cursor position is also the canonical location used in error reports. Every `ParseError` carries a `pos` field of type `std::size_t`, and the convention throughout the library is that this position refers to the same index space as `ParsecInput::pos`. When a parser fails without consuming, the error position is typically the cursor position at entry.

## Consuming Input and Capturing Spans

`consume()` is the primary mutator. It returns the character at the current position and advances `pos` by one, or returns `'\0'` and leaves `pos` unchanged if already at end-of-input. A parser that matches a single character therefore looks like: peek to test, then consume to accept.

```cpp
auto p = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<char> {
  if (in.peek() == ';')
    return in.consume();
  return dsl::fail_expected<char>(in, "semicolon");
});
```

The expression `in.consume()` returns a `char`, which is implicitly converted to `ExpectedResult<char>` via the constructor `ExpectedResult(T)`. This is the most common shape for a primitive parser: test a predicate on `peek()`, return `consume()` on success, return `fail_expected` on failure.

For multi-character tokens, a parser consumes in a loop and then calls `get_span(start)` to recover the matched text. `get_span` returns the substring of `source` from `start` up to the current `pos`.

```cpp
auto ident = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<std::string> {
  std::size_t start = in.pos;
  while (!in.eof() && (std::isalpha(static_cast<unsigned char>(in.peek()))))
    in.consume();
  if (in.pos == start)
    return dsl::fail_expected<std::string>(in, "identifier");
  return std::string(in.get_span(start));
});
```

Because `pos` is public, a parser may also advance it directly when bulk movement is more convenient than repeated `consume()` calls. The combinators themselves use direct `pos` assignment for save and restore, e.g. `in.pos = save;`. Direct assignment is appropriate for backtracking; it is not appropriate as a substitute for `consume()` in ordinary single-character matching, because the return value of `consume()` is frequently the value the parser wants to return.

## ParseError: Structured Failure

A parser failure is not an exception. It is a value of type `ParseError`, which records where the failure happened, how committed it is, and what the parser expected to find.

```cpp
enum class ParseFailureKind
{
  Soft,
  Committed
};

struct ParseError
{
  std::size_t pos{ 0 };
  ParseFailureKind kind{ ParseFailureKind::Soft };
  std::vector<std::string> expected{};
};
```

The `pos` field is the source position associated with the failure. The `kind` field classifies the failure as either `Soft` or `Committed`, a distinction explained in the next section. The `expected` field is a vector of human-readable labels describing what the parser would have accepted.

`ParseError` is default-constructible to a soft failure at position zero with no expected labels. This default is used as the starting accumulator in combinators that merge errors across alternatives. A default-constructed `ParseError` is not itself a meaningful diagnostic — it is a neutral element for the merge operation.

The `expected` vector is a sequence rather than a set, and the merge helper preserves insertion order while suppressing duplicates. This means the order in which alternatives are tried is reflected in the order of labels in a reported error, which is usually the order in which the grammar lists them.

Errors accumulate through the free function `merge_error`, which combines two `ParseError` values into one.

```cpp
inline void
merge_error (ParseError &dst, const ParseError &src)
{
  if (src.pos > dst.pos)
    {
      dst = src;
      return;
    }
  if (src.pos < dst.pos)
    return;
  if (src.kind == ParseFailureKind::Committed)
    dst.kind = ParseFailureKind::Committed;
  for (const auto &label : src.expected)
    {
      if (std::find (dst.expected.begin (), dst.expected.end (), label)
          == dst.expected.end ())
        dst.expected.push_back (label);
    }
}
```

The merge rule is positional. The error that progressed furthest wins, on the principle that the deepest failure is the most informative. When two errors share the same position, their expected-label sets are unioned and the result is `Committed` if either input was `Committed`. This is the mechanism by which a sequence of alternatives produces a single diagnostic listing every acceptable token at the furthest point of failure.

## ParseFailureKind: Soft vs Committed

`ParseFailureKind` distinguishes two kinds of failure. A `Soft` failure is one where the parser tried something, it did not work out, and backtracking is acceptable — the caller may legitimately try another alternative. A `Committed` failure is one where the parser had already consumed input and committed to a branch, so backtracking is not appropriate.

This distinction is what prevents the alternative combinator `|` from silently swallowing failures that should be fatal. When `a | b` is evaluated and `a` fails softly, the combinator restores the cursor and tries `b`. When `a` fails with `Committed`, the combinator returns the failure immediately without trying `b`.

The sequence combinator `&` promotes a soft failure to committed when the first operand had already consumed input before the second operand failed. The relevant lines in the header capture the rule precisely: if the sequence consumed input and the second operand failed, the failure kind becomes `Committed`. This prevents a later alternative from recovering after a partial match that the grammar intended to be committed.

The repetition combinator `*` treats a committed failure inside the loop as fatal to the whole repetition, while a soft failure simply terminates the loop and returns what was collected. The `optional` combinator similarly treats a committed failure as fatal and a soft failure as "no match, but proceed."

Authors of hand-written primitive parsers should default to `Soft` failures, which is what `fail_expected` produces when called with no kind argument. Producing a `Committed` failure from a primitive is rare; commitment is usually a property introduced by combinators that have observed consumption.

## ExpectedResult<T>: Value and Diagnostics

`ExpectedResult<T>` is the return type of every parser. It pairs an optional parsed value with a `ParseError`, so that even a successful parse carries diagnostic context and even a failed parse carries structured information about what went wrong.

```cpp
template <typename T> struct ExpectedResult
{
  using value_type = T;

  std::optional<T> value{};
  ParseError error{};

  ExpectedResult () = default;
  ExpectedResult (std::nullopt_t) : value (std::nullopt) {}
  ExpectedResult (const T &v) : value (v) {}
  ExpectedResult (T &&v) : value (std::move (v)) {}

  static ExpectedResult
  failure (std::size_t pos, ParseFailureKind kind = ParseFailureKind::Soft,
           std::vector<std::string> expected = {})
  {
    ExpectedResult out{};
    out.error = ParseError{ pos, kind, std::move (expected) };
    return out;
  }

  explicit operator bool () const { return value.has_value (); }
  T &operator* () { return *value; }
  const T &operator* () const { return *value; }
};
```

The type has a `value_type` alias, which `dsl::parser` uses to deduce `T` from a lambda's return type. The `value` member is an `std::optional<T>`, empty on failure and populated on success. The `error` member is always present — a default-constructed `ParseError` on success, a meaningful one on failure.

The constructors make success ergonomic. A parser that has a value simply returns it, and the implicit conversion from `T` to `ExpectedResult<T>` does the right thing. A parser that has consumed a character can write `return in.consume();` and the `char` is wrapped. The `failure` static factory is the counterpart for constructing an explicit failure with a position, a kind, and a list of expected labels.

The truthiness test is `explicit operator bool`, which queries `value.has_value()`. This means `if (r)` checks success, while `if (!r)` checks failure. The dereference operator `*` returns a reference to the contained value; dereferencing a failed result is undefined behavior because the optional is empty, so callers should always check first.

Note the design decision: the `error` field is not cleared on success. A parser constructed as `ExpectedResult{some_value}` has a default-constructed `ParseError` (position zero, soft, empty expected). The combinators do not rely on the error field of a successful result; they accumulate errors only from failed sub-results. The field's presence on success is a consequence of keeping the type simple rather than a tagged union.

## Inspecting a Result

Caller code typically inspects an `ExpectedResult` in one of two ways. The first is the boolean test, used when only success or failure matters.

```cpp
auto r = some_parser(in);
if (r) {
  use(*r);
} else {
  report(r.error);
}
```

The second is direct inspection of the `value` and `error` members, used when the diagnostic must be preserved regardless of outcome.

```cpp
auto r = some_parser(in);
if (r.value) {
  use(*r.value);
}
// r.error is always available for reporting
```

The two styles are equivalent because `operator bool` is exactly `value.has_value()`. The first is preferred for its brevity; the second is useful when a function wants to forward both the value and the error onward without branching.

The dereference operator is provided in both const and non-const overloads. The non-const overload returns `T&`, allowing in-place mutation of the parsed value before it is consumed by the next combinator. The const overload returns `const T&`. Neither performs a copy beyond what the optional already owns.

## The Parser<T> Model

`Parser<T>` is the type that wraps a parser callable. It is a struct with a single member, a `std::function` holding the parse logic.

```cpp
template <typename T> struct Parser
{
  std::function<ExpectedResult<T> (ParsecInput &)> parse;
  ExpectedResult<T>
  operator() (ParsecInput &input) const
  {
    return parse (input);
  }
};
```

The `parse` member holds the callable. The `operator()` forwards to it, so a `Parser<T>` is itself callable: given a `ParsecInput&`, it returns an `ExpectedResult<T>`. This is what makes parsers composable — every combinator receives `Parser` objects and produces a new `Parser` whose `parse` member captures the originals by value.

`Parser<T>` is copyable because `std::function` is copyable. The combinators exploit this by capturing parsers by value in their lambdas, which means a composed parser owns copies of its sub-parsers. There is no shared state between a parser and its copies; each invocation is independent.

The library does not expose a concept named `Parser` constraining the callable. The model is structural: anything that can be stored in a `std::function<ExpectedResult<T>(ParsecInput&)>` and invoked with a `ParsecInput&` is a parser. The `dsl::parser` factory is the canonical way to lift a lambda into a `Parser<T>`.

A `Parser<T>` may be default-constructed, leaving `parse` empty. Calling a default-constructed parser throws `std::bad_function_call`, the standard behavior of an empty `std::function`. The library's own factories always initialize `parse`, so this case arises only from user error.

## The parser() Factory

The free function template `dsl::parser` is the bridge from a raw lambda to a `Parser<T>`. It deduces `T` from the lambda's return type so that the caller does not have to spell it out.

```cpp
template <typename Fn>
auto
parser (Fn &&fn)
{
  using Ret = decltype (fn (std::declval<ParsecInput &> ()));
  using T = typename Ret::value_type;
  return Parser<T>{ std::forward<Fn> (fn) };
}
```

The factory requires that the lambda's return type be an `ExpectedResult<U>` for some `U`, because it extracts `value_type` from it. A lambda returning `char` or `std::string` directly will not compile; the caller must wrap the value in an `ExpectedResult` (or rely on the implicit conversion, which works only when the return type is exactly `ExpectedResult<T>`).

In practice, the implicit conversion makes the common case pleasant. A lambda whose declared return type is `ExpectedResult<char>` may return a bare `char`, and the converting constructor takes care of the wrapping. The declared return type is what the factory inspects, so it must be written out.

```cpp
auto digit = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<char> {
  if (!in.eof() && in.peek() >= '0' && in.peek() <= '9')
    return in.consume();
  return dsl::fail_expected<char>(in, "digit");
});
```

The factory perfect-forwards the lambda into the `std::function`. For capturing lambdas this means the captures are moved or copied into the function object according to the lambda's own copy/move behavior. A parser that captures heavy state by value will be expensive to copy; the combinators copy parsers freely, so authors of stateful primitives should keep captured state small.

## fail_expected and labeled

Two helper templates make primitive parsers easier to write and easier to diagnose. The first, `fail_expected`, constructs a failure result from a cursor and a label.

```cpp
template <typename T>
ExpectedResult<T>
fail_expected (const ParsecInput &in, std::string label,
               ParseFailureKind kind = ParseFailureKind::Soft)
{
  return ExpectedResult<T>::failure (in.pos, kind, { std::move (label) });
}
```

It reads the cursor's current position and produces a soft failure by default with a single expected label. It is the standard way for a primitive parser to report that it did not match. The position is taken from the cursor at the point of failure, which is why primitives should not advance the cursor before deciding to fail.

The second helper, `labeled`, wraps a parser so that any failure it produces is reported under a single human-readable label rather than the labels contributed by its internals.

```cpp
template <typename T>
Parser<T>
labeled (const Parser<T> &p, std::string expected)
{
  return parser ([p, expected = std::move (expected)] (
                     ParsecInput &in) -> ExpectedResult<T> {
    auto r = p (in);
    if (!r)
      r.error.expected = { expected };
    return r;
  });
}
```

`labeled` does not change success or failure outcomes; it rewrites the `expected` list of a failed result to a single label. This is how `dsl::ch('a')` reports its failure as expecting `"a"` regardless of how its inner callable is structured. The built-in `ch` and `satisfy` primitives both wrap their implementations in `labeled`.

## The Built-In ch and satisfy Primitives

The header ships two complete primitive parsers that illustrate every concept introduced so far. `ch` matches a single literal character.

```cpp
inline Parser<char>
ch (char c)
{
  return labeled (
      parser ([c] (ParsecInput &in) -> ExpectedResult<char> {
        if (in.peek () == c)
          return in.consume ();
        return fail_expected<char> (in, std::string (1, c));
      }),
      std::string (1, c));
}
```

The structure is canonical: peek, test, consume on success, `fail_expected` on failure, and wrap the whole in `labeled` so the diagnostic reads as the character itself. The label is the one-character string form of `c`.

`satisfy` generalizes `ch` to any predicate.

```cpp
inline Parser<char>
satisfy (std::function<bool (char)> pred, std::string label)
{
  return labeled (
      parser ([pred = std::move (pred), label] (
                  ParsecInput &in) -> ExpectedResult<char> {
        if (!in.eof () && pred (in.peek ()))
          return in.consume ();
        return fail_expected<char> (in, label);
      }),
      std::move (label));
}
```

Note the guard `!in.eof()` before invoking the predicate. This is the pattern recommended earlier: check end-of-input first, so that a predicate never has to interpret the `'\0'` sentinel as a real character. The label is forwarded to `labeled` so that failures of `satisfy` report the predicate's name, not the anonymous internals.

These two primitives are the model to follow when writing custom parsers. A custom parser that returns a richer type than `char` follows the same shape but constructs its `ExpectedResult<U>` from whatever value it builds.

## Running a Parser: run_parser

A parser is driven by passing it a `ParsecInput`. The free function `dsl::run_parser` is the high-level entry point that constructs the cursor from a `string_view`, runs the parser, and applies a final end-of-input check.

```cpp
template <typename T>
struct ParseOutcome
{
  std::optional<T> value{};
  ParseError error{};
};

template <typename T>
ParseOutcome<T>
run_parser (const Parser<T> &p, std::string_view source)
{
  ParsecInput in{ source, 0 };
  auto r = p (in);
  if (!r)
    return { std::nullopt, r.error };
  if (!in.eof ())
    {
      ParseError trailing{ in.pos, ParseFailureKind::Committed,
                           { "<end-of-input>" } };
      return { std::nullopt, trailing };
    }
  return { std::move (r.value), {} };
}
```

`run_parser` returns a `ParseOutcome<T>`, which is structurally similar to `ExpectedResult<T>` but is a distinct type used only at the top level. Its `value` is `std::optional<T>` and its `error` is `ParseError`, the same members as `ExpectedResult` but in a separate type so that top-level results are not confused with intermediate parser results.

Three outcomes are possible. If the parser fails, the outcome carries `nullopt` and the parser's error. If the parser succeeds but the cursor is not at end-of-input, the outcome is a committed failure with the label `"<end-of-input>"`, signaling trailing input that the parser did not consume. If the parser succeeds and the cursor is at end-of-input, the outcome carries the parsed value and an empty error.

The trailing-input check is important. Without it, a parser that matches only a prefix would silently succeed and discard the rest of the source. By treating non-consumed trailing input as a committed failure, `run_parser` enforces the expectation that a top-level parse accounts for the entire input. Parsers intended to consume a prefix should be combined with a recovery or continuation parser rather than run directly through `run_parser`.

## A First Parser: Single Character

The simplest end-to-end example runs `dsl::ch` on a one-character input.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto p = dsl::ch('a');
  auto out = dsl::run_parser(p, "a");
  if (out.value)
    std::cout << "matched: " << *out.value << "\n";
  else
    std::cout << "error at " << out.error.pos << "\n";
}
```

The parser `ch('a')` succeeds on the input `"a"`, consumes the single character, leaves the cursor at end-of-input, and `run_parser` returns an outcome whose `value` holds `'a'`. The `if (out.value)` test distinguishes success from failure, and `*out.value` extracts the matched character.

This example is the minimal pattern for using the parser core: build a parser, run it with `run_parser`, and inspect the `ParseOutcome`. Everything else — composition, diagnostics, custom primitives — builds on this skeleton.

## Inspecting Success and Failure

Changing the input to one that does not match exercises the failure path.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto p = dsl::ch('a');
  auto out = dsl::run_parser(p, "b");
  if (!out.value) {
    std::cout << "error at position " << out.error.pos << "\n";
    std::cout << "expected:";
    for (const auto &label : out.error.expected)
      std::cout << " " << label;
    std::cout << "\n";
  }
}
```

Here `ch('a')` peeks `'b'`, fails to match, and returns a soft failure at position zero with expected label `"a"`. The cursor is not advanced. `run_parser` sees the failure and returns it as the outcome's error. The diagnostic printed is `error at position 0` followed by `expected: a`.

This is the shape of every diagnostic produced by the core: a position and a list of expected labels. The combinators of Chapter 24 will produce richer diagnostics by merging errors from multiple alternatives, but the format at the top level is always the same.

## Building Literal Parsers

A multi-character literal is a sequence of single-character parsers. The sequence combinator `&` is documented in Chapter 24; here it is used only to demonstrate that composed parsers run through the same `run_parser` entry point.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto p = dsl::ch('a') & dsl::ch('b');
  auto out = dsl::run_parser(p, "ab");
  std::cout << (out.value.has_value() ? "ok" : "err") << "\n";
}
```

The sequence `ch('a') & ch('b')` produces a `Parser<std::pair<char,char>>`. On input `"ab"` it succeeds, consuming both characters. On input `"ax"` it fails at position one with expected label `"b"`. The `run_parser` outcome's `value` holds the pair on success and is empty on failure.

A hand-written literal parser that returns the matched text rather than a pair of characters follows the loop-and-`get_span` pattern shown earlier in the identifier example. The combinator-based approach is usually clearer and is the recommended style once Chapter 24's primitives are available.

## A Custom Predicate Parser

The `satisfy` primitive handles most character-class matching without requiring a hand-written parser. A vowel matcher is a one-liner.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto vowel = dsl::satisfy(
      [](char c) { return c == 'a' || c == 'e' || c == 'i'
                        || c == 'o' || c == 'u'; },
      "vowel");
  auto out = dsl::run_parser(vowel, "e");
  if (out.value)
    std::cout << "matched vowel " << *out.value << "\n";
}
```

The predicate is wrapped in a `std::function<bool(char)>`, so a stateless lambda converts implicitly. The label `"vowel"` is what `labeled` will substitute into any failure's expected list. Running this on input `"e"` succeeds and reports the matched character; running it on `"x"` fails at position zero with `expected: vowel`.

This example demonstrates the division of labor between the core and the primitives. The core provides `ParsecInput`, `ExpectedResult`, and `Parser`; the primitives like `satisfy` provide convenient parameterized parsers built on the core; the combinators of Chapter 24 compose primitives into grammars.

## Driving a Parser by Hand

`run_parser` is convenient but opinionated: it requires the parser to consume the entire input. For cases where a prefix parse is intended, a caller may construct a `ParsecInput` directly and invoke the parser.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  dsl::ParsecInput in{ "abc", 0 };
  auto p = dsl::ch('a');
  auto r = p(in);
  if (r) {
    std::cout << "matched " << *r << ", remaining: "
              << in.source.substr(in.pos) << "\n";
  }
}
```

Here the parser consumes only the first character. The remaining input `"bc"` is available as `in.source.substr(in.pos)`. The result `r` is the raw `ExpectedResult<char>`; no `ParseOutcome` is constructed because `run_parser` was bypassed. This pattern is useful when a parser is embedded in a larger system that manages its own input framing, such as the token-stream integration sketched in `examples/15-tokenize-parse-integration.cpp`.

Direct construction of `ParsecInput` is also how recursive descent with embedded combinator parsers is implemented: the outer driver maintains the cursor and calls sub-parsers on it, threading the same `ParsecInput&` through the recursion.

## Value Semantics and Copyability

Parsers are values. A `Parser<T>` is copyable, and copying copies the `std::function` it holds. The combinators capture parsers by value in the lambdas they construct, which means a composed parser owns its sub-parsers outright. There is no need to manage lifetimes, no shared pointers, no `this` captured into a combinator that outlives its origin.

The price of this simplicity is that a parser with large captured state is expensive to copy, and the combinators copy freely. In practice, primitives capture at most a `char`, a predicate, and a label, so the cost is negligible. A parser that needs to capture a large table should consider storing the table in a `std::shared_ptr` and capturing the shared pointer, so that copies of the parser share the table rather than duplicating it.

`ParsecInput`, by contrast, is not passed by value. It is passed by reference, because it is the mutable state of the parse in progress. Copying a `ParsecInput` would snapshot the cursor, which is occasionally useful for speculative parsing but is not what the ordinary parser signature expects. The combinators that need to snapshot copy only the `pos` member (`auto save = in.pos;`) and restore it (`in.pos = save;`), which is cheaper than copying the whole struct and clearer in intent.

`ExpectedResult<T>` is a value type and is returned by value from every parser. Moves are used where possible: the `failure` factory moves the expected-label vector, and `run_parser` moves the final value into the outcome. For large value types, move semantics keep the cost of returning an `ExpectedResult` low.

## Why Combinators

The core types documented in this chapter are sufficient to write any parser by hand, but writing parsers by hand is tedious. The combinators of Chapter 24 exist to factor out the common patterns: sequencing two parsers, choosing between alternatives, repeating a parser zero or more times, and making a parser optional.

Each combinator is a template function that takes one or two `Parser<T>` objects and returns a new `Parser<U>`. The implementation of each combinator is a lambda that runs its operands with appropriate save-and-restore of the cursor and appropriate merging of errors. The result is a parser in exactly the sense of this chapter — a callable conforming to the `Parser<T>` model — so combinators compose with primitives and with each other without distinction.

The forward references in this chapter — `&` for sequence, `|` for alternative, `*` for repetition, `optional` for optionality — are documented fully in Chapter 24. The error-merging behavior described here through `merge_error` is the mechanism the combinators use internally to produce the diagnostics that `run_parser` ultimately returns. Chapter 25 covers the diagnostic surface in more detail, including how `labeled` and the committed/soft distinction shape the messages a user sees.

## Pitfalls

Several pitfalls await the unwary author of hand-written parsers. The first and most common is forgetting to advance the cursor on success. A parser that returns a value without consuming input will, when combined with the repetition combinator `*`, loop forever: the combinator keeps trying the parser at the same position, keeps succeeding, and never makes progress. Every primitive that succeeds must advance `pos` by at least one, or must be combined in a context that guarantees progress.

The second pitfall is advancing the cursor and then failing. A primitive that consumes input and then decides it cannot complete the match should restore `pos` before returning a soft failure, otherwise a later alternative will start from the wrong position. The combinators handle this for their operands, but a hand-written primitive that consumes speculatively must handle it itself. The `try_parse` wrapper, documented in Chapter 24, exists to restore the cursor on failure; it is the recommended tool for speculative consumption.

The third pitfall is treating `ExpectedResult`'s `error` field as meaningful on success. On a successful parse the `error` is default-constructed — position zero, soft, empty expected — and carries no information. Code that forwards an `ExpectedResult` should not assume the error is populated; it should branch on `operator bool` first, as the examples in this chapter do.

The fourth pitfall is expecting parser failures to throw. They do not. A parser that encounters malformed input returns a failure value, and the caller — typically a combinator or `run_parser` — inspects that value. There is no `try`/`catch` around parser invocation, and the library does not use exceptions for control flow within the parser core. This aligns with the error-flow discipline of Chapter 22's `Result<T,E>`.

The fifth pitfall is letting the source string's storage expire before the parse completes. `ParsecInput` holds a `string_view`, not a `std::string`. If the caller constructs a cursor from a temporary `std::string`, the cursor's `source` dangles after the temporary is destroyed. The caller must keep the underlying storage alive for the duration of the parse, typically by binding the source to a named variable before constructing the cursor or calling `run_parser`.

The sixth pitfall is conflating `ParseOutcome<T>` with `ExpectedResult<T>`. They have the same shape but are different types. `ExpectedResult` is the intermediate result produced by every parser; `ParseOutcome` is the top-level result produced by `run_parser`. Functions that accept one will not accept the other, and the two should not be confused when designing APIs around the parser core.

## Summary

This chapter documented the parser-combinator substrate of DSLtk. `ParsecInput` is the mutable input cursor, holding a `string_view` source and a `pos` index, with `peek`, `eof`, `consume`, and `get_span` operations. `ParseError` is the structured failure record, carrying a position, a soft-or-committed kind, and a list of expected labels, merged by `merge_error` according to furthest-position-wins semantics. `ExpectedResult<T>` pairs an `std::optional<T>` value with a `ParseError`, serving as the universal return type of parsers. `Parser<T>` wraps a `std::function` and is itself callable, and the `dsl::parser` factory lifts lambdas into this model. `dsl::run_parser` is the top-level entry point, seeding a cursor from a `string_view`, running the parser, enforcing end-of-input, and returning a `ParseOutcome<T>`.

The core is deliberately minimal: it provides value-semantic parsers, structured best-effort diagnostics, and a clean separation between input state and parse results. Failures are values, not exceptions, consistent with the error-flow discipline of Chapter 22. The combinators that turn these primitives into a grammar — sequence, alternative, repetition, optional — are the subject of Chapter 24. Diagnostics, semantic actions, and the broader `run_parser` story are covered in Chapter 25. The PEG layers in Chapter 27 and Chapter 28 build specialized grammars on top of the same core.
