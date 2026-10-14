# Chapter 24: Primitive Parsers and Basic Combinators

Chapter 23 introduced the combinator-parser core of DSLtk: the `ParsecInput` cursor, the `Parser<T>` wrapper, and the `ExpectedResult<T>` value type that carries either a parsed value or a `ParseError`. This chapter opens up the box of primitive parsers and combinators that ship in the `dsl` namespace and shows how they compose into recognisers for non-trivial grammars. The treatment is deliberately concrete. Every code fragment uses the real `dsl::` API and compiles against a C++20 toolchain with `DSLtk.hpp` included.

The combinators described here form the substrate on which the heavier machinery of Chapter 25 (diagnostics, semantic actions, `run_parser` plumbing) and Chapter 27 (the PEG definition grammar) is built. A reader who works through the worked examples will already be able to write small recursive-descent recognisers for identifiers, numbers, and `key=value` pairs before semantic actions are introduced. Where the chapter touches on the run pipeline or production handlers, it forward-references Chapter 25 rather than duplicating its material.

## The Parser Abstraction in Brief

A `Parser<T>` is a wrapper around `std::function<ExpectedResult<T>(ParsecInput&)>`. Constructing one is a matter of handing a callable to `dsl::parser`, which deduces `T` from the callable's return type. The callable receives the mutable `ParsecInput` by reference, advances `in.pos` as it consumes input, and returns either a value or a failure. This is the only contract a primitive parser must honour.

The `ParsecInput` struct exposes three primitives used throughout this chapter: `peek()` returns the character under the cursor or `'\0'` at end-of-file, `consume()` returns the character under the cursor and advances, and `eof()` reports whether the cursor has reached the end. A parser that matches one character typically calls `peek()` to test, then `consume()` to accept. The cursor is the sole carrier of mutable state, which makes backtracking trivial: restoring `in.pos` to a saved value undoes every side effect of a speculative parse.

Failures carry a `ParseFailureKind` of either `Soft` or `Committed`. The distinction governs how the choice combinator `|` behaves, and it is examined in detail later in this chapter. For now it suffices to know that a `Soft` failure is a speculative miss from which the combinator core may backtrack, while a `Committed` failure signals that input was consumed and the alternative branch should not be tried. The primitives presented in the next section all fail softly.

## Primitive: Matching a Single Character

The two single-character primitives are `dsl::ch` and `dsl::satisfy`. The first matches one specific character; the second matches one character that satisfies a predicate. Between them they cover every common case of single-token recognition, including the character classes that other libraries ship as named parsers.

`dsl::ch(c)` returns a `Parser<char>` that succeeds when the character under the cursor equals `c`, consuming it, and fails softly otherwise. Its definition is short enough to quote verbatim.

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

The `labeled` wrapper, examined later in this chapter, attaches the character itself as the "expected" label so that diagnostic messages read naturally. A `Parser<char>` is the smallest useful building block in the library; almost every grammar in the examples begins by combining `ch` with itself or with `satisfy`.

The second primitive, `dsl::satisfy(pred, label)`, generalises `ch` to an arbitrary predicate on the peeked character. It is the canonical way to express a character class. Its definition is:

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

The `label` argument is the string reported in errors when the predicate fails. Passing `"digit"`, `"letter"`, or `"whitespace"` makes diagnostics self-documenting. The predicate is stored by value in the captured lambda, so a `satisfy`-based parser is copyable and storable in a `Parser<char>`.

## Character Classes as Combinators

DSLtk does not ship named `dsl::digit` or `dsl::letter` parsers. The intended idiom, demonstrated in `examples/05-parser-basics.cpp` and `examples/07-parser-optional-repeat-choice.cpp`, is to define them at the call site with `satisfy`. This keeps the primitive set small and the label string under the grammar author's control.

```cpp
auto digit  = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
auto letter = dsl::satisfy([](char c){ return c >= 'a' && c <= 'z'; }, "letter");
auto upper  = dsl::satisfy([](char c){ return c >= 'A' && c <= 'Z'; }, "upper");
auto alnum  = dsl::satisfy([](char c){ return std::isalnum(static_cast<unsigned char>(c)); }, "alnum");
auto ws     = dsl::satisfy([](char c){ return c == ' ' || c == '\t' || c == '\n'; }, "whitespace");
```

Each of these is a `Parser<char>`. They compose with the operators introduced later in exactly the same way that `ch` does. The naming is purely local: `digit` is a variable, not a member of the `dsl` namespace, which is why the examples always spell it in lower case at point of use.

The any-character parser sometimes called `item` in other combinator libraries is expressed by passing a predicate that always returns `true`. Because `satisfy` already guards against end-of-input, this yields a parser that consumes exactly one character when one is available and fails softly at EOF.

```cpp
auto item = dsl::satisfy([](char){ return true; }, "any character");
```

This idiom is rarely needed in practice — grammars usually discriminate on character class — but it is the right tool when a grammar must consume and echo an arbitrary byte, for instance inside a fallback rule.

## Matching a Literal String

There is no built-in `dsl::string_` primitive. A literal-string recogniser is assembled from `ch` and the sequence and repetition combinators introduced below, or — more compactly — from `satisfy` plus a manual equality test. A typical definition folds a `std::string_view` into a parser that consumes the whole run or fails softly without consuming anything.

```cpp
dsl::Parser<std::string> string_(std::string_view s) {
  return dsl::parser([s](dsl::ParsecInput& in) -> dsl::ExpectedResult<std::string> {
    for (char c : s) {
      if (in.peek() != c)
        return dsl::fail_expected<std::string>(in, std::string(s));
      in.consume();
    }
    return std::string(s);
  });
}
```

This helper consumes input only when the full literal matches; on a mismatch it restores nothing because the loop body returns before consuming. A more conservative variant would snapshot `in.pos` at entry and restore it on failure, so that a partial match leaves the cursor untouched. The sequence combinator `&` already provides that guarantee when a literal is built by folding `ch` over the characters of the string, which is the approach used in the worked examples later in this chapter.

The `Parser<std::string>` return type is deliberate: a string matched character-by-character via `ch` would otherwise accumulate as a `std::pair<char, std::pair<char, ...>>` nesting under `&`, which is awkward to use. Returning a `std::string` directly from a single primitive keeps the value type flat.

## Repetition: The `*` Operator

Repetition of zero or more matches is expressed by the unary `operator*`. Applying `*p` yields a `Parser<std::vector<T>>` that runs `p` repeatedly until it fails, collecting each successful value into a vector. The combinator always succeeds, even when `p` matches zero times — the resulting vector is simply empty.

The definition is worth quoting in full, because its control flow encodes the library's policy on committed failures and non-progress.

```cpp
template <typename T>
Parser<std::vector<T>>
operator* (const Parser<T> &p)
{
  return parser (
      [p] (ParsecInput &in) -> ExpectedResult<std::vector<T>>
        {
          std::vector<T> out;
          ParseError best_err{};
          while (true)
            {
              std::size_t save = in.pos;
              auto r = p (in);
              if (!r)
                {
                  merge_error (best_err, r.error);
                  if (r.error.kind == ParseFailureKind::Committed)
                    return ExpectedResult<std::vector<T>>::failure (
                        best_err.pos, best_err.kind, best_err.expected);
                  in.pos = save;
                  break;
                }
              out.push_back (*r);
            }
          return out;
        });
}
```

Two properties deserve emphasis. First, on a soft failure the cursor is restored to `save` before the loop breaks, so a trailing partial match leaves no residue. Second, a committed failure aborts the whole repetition and propagates the committed status; this is how a sequence inside the repeated parser can short-circuit the loop. The accumulated `best_err` is merged so that the deepest speculative failure is the one reported.

A direct consequence of the loop structure is that a parser `p` which succeeds without consuming input will loop forever. The combinator performs no progress check; the grammar author is responsible for ensuring that every operand of `*` consumes at least one character on success. This pitfall is revisited at the end of the chapter.

## One-or-More Repetition

The core combinators expose only the Kleene star (`*p`, zero or more). The plus repetition — one or more matches — is expressed as a sequence of one mandatory match followed by zero or more optional matches. Because `&` combines two results into a `std::pair`, the idiomatic `many1` flattens the pair into a single vector.

```cpp
template <typename T>
dsl::Parser<std::vector<T>> many1(const dsl::Parser<T>& p) {
  return dsl::parser([p](dsl::ParsecInput& in) -> dsl::ExpectedResult<std::vector<T>> {
    auto first = p(in);
    if (!first)
      return dsl::ExpectedResult<std::vector<T>>::failure(
          first.error.pos, first.error.kind, first.error.expected);
    std::vector<T> out;
    out.push_back(*first);
    auto rest = *p;
    auto r = rest(in);
    if (!r)
      return dsl::ExpectedResult<std::vector<T>>::failure(
          r.error.pos, r.error.kind, r.error.expected);
    for (auto& v : *r) out.push_back(std::move(v));
    return out;
  });
}
```

With this helper in scope, `many1(digit)` parses `42` into a vector of two characters and fails on an empty input. The failure is soft when `digit` fails softly, which means a `many1` operand can be used as the first alternative in a `|` choice without committing the input.

The PEG layer introduced in Chapter 27 ships its own `dsl::peg_many` and `dsl::peg_many1` with identical semantics but additional channel bookkeeping; readers who reach for repetition inside a PEG definition should prefer those. This chapter's `many1` is the appropriate tool when working directly with the core combinators.

## Optional Parsing

`dsl::optional(p)` wraps a parser so that a soft failure is reported as success with an empty `std::optional<T>`. A committed failure propagates unchanged, preserving the contract that committed input cannot be backtracked. The return type is `Parser<std::optional<T>>`.

```cpp
template <typename T>
Parser<std::optional<T>>
optional (const Parser<T> &p)
{
  return parser (
      [p] (ParsecInput &in) -> ExpectedResult<std::optional<T>>
        {
          auto save = in.pos;
          auto r = p (in);
          if (!r)
            {
              if (r.error.kind == ParseFailureKind::Committed)
                return ExpectedResult<std::optional<T>>::failure (
                    r.error.pos, r.error.kind, r.error.expected);
              in.pos = save;
              return std::optional<T>{};
            }
          return std::optional<T>{ *r };
        });
}
```

The snapshot-and-restore on the soft-failure path means `optional` never leaves the cursor advanced when it returns an empty optional. This is essential for idioms like an optional sign character followed by digits: if the sign is absent, the digit parser must see the input from the original position.

```cpp
auto sign = dsl::optional(dsl::ch('-'));
auto digit = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
auto num = sign & *digit;
```

The type of `num` is `Parser<std::pair<std::optional<char>, std::vector<char>>>`. The sign, when present, is recovered by dereferencing the first element of the pair. Example 07 in the distribution uses exactly this shape to parse `-42`.

## Sequencing Parsers: The `&` Operator

The binary `operator&` expresses sequencing. The expression `a & b` produces a `Parser<std::pair<A, B>>` that runs `a`, then — on success — runs `b` against the advanced cursor, and returns the two results as a pair. Failure of either operand aborts the sequence.

```cpp
template <typename A, typename B>
Parser<std::pair<A, B>>
operator& (const Parser<A> &a, const Parser<B> &b)
{
  return parser (
      [a, b] (ParsecInput &in) -> ExpectedResult<std::pair<A, B>>
        {
          auto save = in.pos;
          auto ra = a (in);
          if (!ra)
            return ExpectedResult<std::pair<A, B>>::failure (
                ra.error.pos, ra.error.kind, ra.error.expected);
          auto rb = b (in);
          if (!rb)
            {
              bool consumed = in.pos != save;
              in.pos = save;
              ParseFailureKind kind = rb.error.kind;
              if (consumed)
                kind = ParseFailureKind::Committed;
              return ExpectedResult<std::pair<A, B>>::failure (
                  rb.error.pos, kind, rb.error.expected);
            }
          return std::make_pair (*ra, *rb);
        });
}
```

Two details shape the practical use of `&`. First, when `b` fails after `a` consumed input, the sequence promotes the failure to `Committed` before returning it; this prevents a surrounding `|` from silently backtracking over the consumed prefix. Second, the cursor is always restored to `save` on failure, so a failed sequence leaves no residue.

Because `&` nests to the right, chaining three parsers produces a `std::pair<A, std::pair<B, C>>`. Grammars that need a flat value typically post-process the pair with a `map`-style transform or build a custom parser that returns the desired aggregate. The AST-construction combinators of Chapter 11 are the recommended tool when the goal is a tree rather than a tuple.

## Ordered Choice: The `|` Operator

The binary `operator|` expresses ordered choice. The expression `a | b` runs `a`; if `a` succeeds, its result is returned. If `a` fails softly, the cursor is restored and `b` is tried from the same position. If `a` fails committed, the failure is returned without trying `b`. The two operands must have the same result type `T`.

```cpp
template <typename T>
Parser<T>
operator| (const Parser<T> &a, const Parser<T> &b)
{
  return parser (
      [a, b] (ParsecInput &in) -> ExpectedResult<T>
        {
          auto save = in.pos;
          auto ra = a (in);
          if (ra)
            return ra;
          if (ra.error.kind == ParseFailureKind::Committed)
            return ra;
          in.pos = save;
          auto rb = b (in);
          if (!rb)
            merge_error (rb.error, ra.error);
          return rb;
        });
}
```

Choice in DSLtk is ordered, not longest-match. The first operand that succeeds wins, regardless of how much input it consumes. This is the PEG semantic: a grammar `a | b` is read as "attempt `a`; if it fails, attempt `b`". A consequence is that the order of alternatives matters. Placing a more general alternative before a more specific one will shadow the specific one permanently.

When both branches fail, `merge_error` combines the two errors, preferring the one that made the furthest progress and unioning their `expected` labels. This is what produces the multi-label "expected digit or letter" style of diagnostic. The merge rules are examined in Chapter 25; for this chapter it is enough to know that the deeper failure wins.

## Backtracking and the Soft/Committed Distinction

The choice and optional combinators both restore `in.pos` on a soft failure. This is the mechanism of backtracking: a speculative parse is undone by resetting the cursor to a saved value, and the next alternative begins from the same position. No other state needs to be unwound, because `ParsecInput` carries only the cursor and the immutable source view.

A committed failure is the combinator core's way of signalling that backtracking would be incorrect. The sequence combinator `&` commits when its first operand consumed input and its second operand then failed; the repetition combinator `*` propagates a committed failure from its body; `optional` propagates a committed failure rather than converting it to an empty optional. The effect is that once a parser has unambiguously committed to a branch, later alternatives in a `|` are not tried.

This is the classic parsec-style committed-choice discipline. It avoids the incidental backtracking that plagues naive longest-match parsers, while still allowing speculative alternatives at points where the grammar is genuinely ambiguous. Chapter 23 covers the underlying `ParseFailureKind` enum in more detail; Chapter 25 explains how diagnostic labels surface in `ParseError::expected`.

## Relabelling and Softening: `labeled` and `try_parse`

Two small adapters round out the primitive set. `dsl::labeled(p, label)` runs `p` unchanged on success but overwrites the `expected` list of a failure with the single label `label`. It is how `ch` and `satisfy` attach their friendly diagnostic strings. Grammars can use it to name a sub-rule: `labeled(many1(digit), "integer")` will report `expected integer` on failure rather than the default `expected digit`.

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

`dsl::try_parse(p)` is the adapter that demotes a committed failure to a soft one. It snapshots the cursor, runs `p`, and on any failure restores the cursor and rewrites the failure's kind to `Soft`. This is the escape hatch when a parser that would otherwise commit should instead be made speculative — for example, when wrapping a sequence that is the first operand of a `|` and the grammar author wants the second operand to be tried even after partial consumption.

```cpp
template <typename T>
Parser<T>
try_parse (const Parser<T> &p)
{
  return parser ([p] (ParsecInput &in) -> ExpectedResult<T> {
    auto save = in.pos;
    auto r = p (in);
    if (!r)
      {
        in.pos = save;
        r.error.kind = ParseFailureKind::Soft;
      }
    return r;
  });
}
```

Between `labeled` and `try_parse`, a grammar author can control the two things that matter for diagnostics and backtracking: what a rule is called, and whether a failure blocks subsequent alternatives. The combinators `&`, `|`, `*`, and `optional` are sufficient for everything else.

## Running a Parser

`dsl::run_parser(p, source)` is the entry point that turns a `Parser<T>` into a result against a string. It constructs a `ParsecInput` over the supplied `std::string_view`, invokes the parser, and packages the outcome in a `ParseOutcome<T>` — a struct holding an `std::optional<T>` value and a `ParseError`.

```cpp
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

A successful parse must consume the entire input; a trailing suffix produces a committed failure with the label `<end-of-input>`. This is stricter than invoking the parser directly against a `ParsecInput`, which would happily return a value and leave the cursor mid-stream. The strictness is deliberate: a top-level grammar that does not reach end-of-input almost certainly has a hole that should be reported.

The full run pipeline — including source loading from files via `dsl::load_parse_input_file`, line/column resolution for diagnostics, and integration with semantic actions — is the subject of Chapter 25. The minimal `run_parser` shown here is all that the worked examples in this chapter require.

## Worked Example: A Natural Number

Combining the primitives yields a recogniser for a natural number in a few lines. The grammar is one or more digits, flattened into a vector of characters; a production layer (Chapter 25) would convert the vector into an integer.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto digit = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
  auto nat   = *digit;                       // Parser<std::vector<char>>
  auto out   = dsl::run_parser(nat, "4096");
  if (out.value)
    std::cout << "digits: " << out.value->size() << "\n";
  else
    std::cout << "failed at " << out.error.pos << "\n";
}
```

The Kleene star accepts the empty string, so `nat` succeeds on `""` with an empty vector. When the grammar requires at least one digit, the `many1` helper introduced earlier is the right tool. Note that `run_parser` rejects trailing input: parsing `"4096x"` fails at position 4 with the `<end-of-input>` label, because the parser stops at `6` and leaves `x` unconsumed.

## Worked Example: An Identifier

The classic identifier rule `[a-zA-Z_][a-zA-Z_0-9]*` exercises `satisfy`, choice, and repetition together. The first character is a letter or underscore; the tail is zero or more letters, underscores, or digits.

```cpp
auto id_first = dsl::satisfy(
    [](char c){ return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; },
    "identifier-start");
auto id_rest = dsl::satisfy(
    [](char c){ return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; },
    "identifier-continue");
auto identifier = id_first & *id_rest;   // Parser<std::pair<char, std::vector<char>>>
```

The result type is a pair of the head character and a vector of tail characters. A production handler can stitch them into a `std::string` in one line; for ad-hoc use, iterating the pair is straightforward. Because `id_first` fails softly on a non-matching character, `identifier` can be used as one alternative in a larger choice without committing the cursor.

```cpp
auto out = dsl::run_parser(identifier, "foo_bar99");
if (out.value) {
  char head = out.value->first;
  std::string tail(out.value->second.begin(), out.value->second.end());
  std::cout << head << tail << "\n";      // prints foo_bar99
}
```

## Worked Example: A Quoted String

A double-quoted string pairs a literal delimiter with a repetition of any character that is not the closing quote. The `string_` helper is unnecessary for the quotes themselves; `ch` is sufficient.

```cpp
auto quote    = dsl::ch('"');
auto not_quote = dsl::satisfy([](char c){ return c != '"'; }, "string-char");
auto quoted   = quote & *not_quote & quote;
```

The type of `quoted` is `Parser<std::pair<char, std::pair<std::vector<char>, char>>>`. The payload is the middle element of the nested pair; the two quote characters are discarded by any downstream production. Note that `*not_quote` accepts the empty string, so `""` parses as an empty vector, which is the desired behaviour.

A subtlety: `not_quote` does not exclude newlines or backslash escapes. A real string grammar would add an escape alternative using `|`, for instance `('\\' & item) | not_quote`, to handle `\"` and `\\`. The `item` idiom defined earlier in the chapter consumes the character following the backslash without discrimination.

## Worked Example: `key=value`

A `key=value` record combines an identifier, a literal `=`, and a value. Sequencing with `&` builds the shape directly.

```cpp
auto key   = identifier;                       // from the previous example
auto eq    = dsl::ch('=');
auto value = *dsl::satisfy([](char c){ return c != ';' && c != '\n'; }, "value-char");
auto record = key & eq & value;
```

The `record` parser succeeds on `foo=bar` and fails on `foo=`, because `value` accepts the empty tail but `eq` requires the `=`. Trailing whitespace and semicolons are rejected by `run_parser`'s end-of-input check, which is usually the desired behaviour for a line-oriented record grammar.

## Worked Example: A Comma-Separated List

Comma-separated lists are the canonical application of the `*` operator over a sequence. The grammar is: one element, followed by zero or more `(comma element)` pairs. The element parser is a parameter, so the list combinator is generic.

```cpp
template <typename T>
dsl::Parser<std::vector<T>> sep_by1(const dsl::Parser<T>& elem, char sep) {
  auto comma_elem = dsl::ch(sep) & elem;
  return dsl::parser(
      [elem, comma_elem](dsl::ParsecInput& in) -> dsl::ExpectedResult<std::vector<T>> {
        auto first = elem(in);
        if (!first)
          return dsl::ExpectedResult<std::vector<T>>::failure(
              first.error.pos, first.error.kind, first.error.expected);
        std::vector<T> out;
        out.push_back(*first);
        auto rest = *comma_elem;
        auto r = rest(in);
        if (r) for (auto& p : *r) out.push_back(std::move(p.second));
        return out;
      });
}
```

The `comma_elem` parser has type `Parser<std::pair<char, T>>`; the second element of each pair is the value to keep. Applying `*` to it yields a vector of pairs, from which the list combinator extracts the `.second` of each. Used as `sep_by1(digit, ',')`, this parses `1,2,3` into a vector of three characters.

A `sep_by` variant that accepts the empty list is built by wrapping `sep_by1` in `optional` and flattening. Because `optional` returns `std::optional<std::vector<T>>`, the empty case is recoverable by checking `has_value` before dereferencing.

## The `parse_with_action` Bridge

The core combinators return plain values. Attaching a semantic action — a callback fired on a successful match with the matched span in hand — is the role of `dsl::parse_with_action`. It runs a parser, records the begin and end positions, and invokes the action with a `ParserStage` describing the matched span.

```cpp
template <typename T, typename ActionFn>
auto
parse_with_action (
    const Parser<T> &p, ParsecInput &input,
    const std::unordered_map<std::string, std::string> &semantics,
    ActionFn &&action)
    -> std::optional<decltype (action (semantics, ParserStage{}))>
{
  std::size_t begin = input.pos;
  auto r = p (input);
  if (!r)
    return std::nullopt;
  ParserStage stage{ begin, input.pos, input.get_span (begin) };
  return action (semantics, stage);
}
```

The `semantics` map is a table of named productions, populated by the table-pass layer described in Chapter 25. This chapter's grammars do not need it; `parse_with_action` is mentioned here only because it is the natural bridge from a flat combinator grammar to the production-handler machinery. A reader who has built a recogniser with the tools above is already one step away from a fully semantic-actioned parser.

The `dsl::production<Id>(fn)` helper, also defined in the same region of the header, registers a handler keyed by a `FixedString` identifier. It returns a `ProductionHandler<Id, Fn>` for the run pipeline to dispatch on. Production handlers are the topic of Chapter 25 and are not used directly in this chapter's examples.

## Pitfall: Left Recursion

The combinator core is PEG-like and evaluates operands eagerly by recursion. A grammar rule that begins with itself — `expr = expr & ch('+') & term | term` — will call itself before consuming any input, and so will recurse without bound until the C++ stack overflows. The library provides no left-recursion detection or guard.

The standard remedy is the same as for any recursive-descent parser: rewrite the rule iteratively. `expr = term & *(ch('+') & term)` recognises the same language without left recursion, and is in fact the form used in the worked examples above. The PEG combinators of Chapter 27 inherit the same restriction; the PEG definition grammar of Chapter 28 provides a separate, non-recursive rule expansion mechanism that does not suffer from this pitfall.

## Pitfall: Non-Progressing Repetition

The `*` combinator's loop body does not check that the operand consumed input. A parser that succeeds without advancing the cursor — for example, `optional(ch('x'))` applied to input that does not start with `x` — will match an empty optional at every iteration, appending nothing to the vector and never terminating.

The defence is grammatical: ensure that every operand of `*` either fails or consumes at least one character. A character-class parser built with `satisfy` satisfies this by construction, since `satisfy` only succeeds by calling `consume()`. The danger arises when wrapping `optional`, `try_parse`, or a hand-written parser that returns success on a lookahead miss. When in doubt, assert progress inside the operand.

## Pitfall: Ordered Choice Is Not Longest Match

The `|` operator returns the first alternative that succeeds, regardless of how much input it consumes. A grammar `keyword | identifier` will work correctly only if `keyword` is listed first; placing `identifier` first would cause every keyword to be parsed as an identifier and shadow the keyword alternative permanently.

This is the PEG semantic, and it is the price of the predictable, backtracking-friendly behaviour described earlier. Where longest-match is genuinely required — for instance, in a lexer that must prefer `<=` over `<` — the remedy is to order alternatives from longest to shortest, or to factor the grammar so that the longer alternative is a strict refinement of the shorter one. Chapter 9's compile-time `pattern<>` engine is the appropriate tool for lexical longest-match concerns; the combinator core is intended for structural grammar above the token layer.

## Summary

This chapter has presented the primitive parsers and basic combinators that ship in the `dsl` namespace. The single-character primitives are `dsl::ch` and `dsl::satisfy`; character classes such as `digit`, `letter`, and the any-character `item` are defined at the call site with `satisfy`. Repetition of zero or more matches is the unary `*` operator, which returns a `Parser<std::vector<T>>`; one-or-more repetition is built from a mandatory match followed by `*`. The `dsl::optional` combinator wraps a parser to accept zero or one occurrence, returning `Parser<std::optional<T>>`.

Sequencing is `operator&`, which returns a `Parser<std::pair<A, B>>` and commits the sequence when the first operand has consumed input. Ordered choice is `operator|`, which tries the left operand and, on a soft failure, restores the cursor and tries the right; the choice is ordered, not longest-match. The `dsl::labeled` and `dsl::try_parse` adapters control diagnostic labels and the soft/committed status of failures. `dsl::run_parser` executes a parser against a string and requires that the entire input be consumed.

The worked examples composed these primitives into a natural-number recogniser, an identifier recogniser, a quoted-string recogniser, a `key=value` record, and a comma-separated list. The chapter closed with the three pitfalls that account for most combinator-grammar bugs: left recursion, non-progressing repetition, and the ordered (non-longest-match) semantics of choice. The next chapter, Chapter 25, builds on this substrate to add diagnostics, semantic actions, and the full `run_parser` pipeline; Chapter 27 and Chapter 28 revisit these combinators in the context of the PEG definition grammar and the PEG matcher.
