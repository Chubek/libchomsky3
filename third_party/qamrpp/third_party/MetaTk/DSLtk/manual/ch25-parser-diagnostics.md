# Chapter 25: Parser Diagnostics, Semantic Actions, and `run_parser`

The previous two chapters built the combinator core of `dsl`'s parser library. Chapter 23 introduced `ParsecInput`, `Parser<T>`, and the `|`/`&`/`*` operators that compose parsers functionally. Chapter 24 added the primitive parsers `ch`, `satisfy`, and the labelling helpers that give a grammar its vocabulary of "expected" tokens. This chapter turns to the questions that arise once a grammar is large enough to fail in interesting ways: how a failed parse surfaces a diagnostic, how the top-level entry point drives a parser to completion, and how a matched production can be intercepted to build an AST or token stream on the fly.

Three concerns dominate. The first is error reporting: a `Parser<T>` returns an `ExpectedResult<T>`, and the `ParseError` carried on failure records the farthest position reached, a soft/committed classification, and the set of labels accumulated across alternatives. The second is the entry point `run_parser`, which seeds a `ParsecInput` from a source string, runs the parser, and refuses to report success when trailing input remains unconsumed. The third is `parse_with_action`, the semantic-action hook that fires a user callback with the matched span whenever a parser succeeds, allowing a grammar to assemble side structures as it recognises input.

Together these facilities close the loop between recognition and use. A parser that only returns `bool` is a recogniser; a parser whose results are turned into structured values is a translator. The material here connects the combinator layer of Chapters 23 and 24 to the AST machinery of Chapter 10 and the PEG layer of Chapters 27 and 28.

## The Diagnostic Surface: `ParseError` and `ExpectedResult`

Every `dsl` parser returns an `ExpectedResult<T>`. The type is a thin envelope around an `std::optional<T>` carrying the matched value and a `ParseError` carrying the diagnostic that applies when the value is absent. The `explicit operator bool()` lets combinators branch on success with the usual idiom, and `operator*` recovers the value. The `ParseError` itself is the unit of diagnostic information that flows up through a combinator tree.

The relevant definitions, quoted verbatim from the header, are as follows.

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

Three fields make up a `ParseError`. The `pos` field is a byte offset into the source string, not a line/column pair; conversion to human-readable coordinates is the caller's responsibility and is discussed below. The `kind` field distinguishes a `Soft` failure, which a surrounding `|` may still recover from by trying the next alternative, from a `Committed` failure, which signals that input has been irrevocably consumed and the alternative branch must not be attempted. The `expected` vector is a list of human-readable labels describing what the parser would have accepted at the failure position.

The `ExpectedResult::failure` factory is the conventional way to construct a failing result inside a primitive parser. It takes a position, an optional kind (defaulting to `Soft`), and an optional vector of expected labels. The `fail_expected` helper wraps this for the common case of a single label derived from the current input position.

```cpp
template <typename T>
ExpectedResult<T>
fail_expected (const ParsecInput &in, std::string label,
               ParseFailureKind kind = ParseFailureKind::Soft)
{
  return ExpectedResult<T>::failure (in.pos, kind, { std::move (label) });
}
```

A primitive that cannot match calls `fail_expected(in, "digit")`, attaching a label that the combinator layer will later surface to the user. The label is the contract between the primitive and the diagnostic layer: the primitive knows what it wanted, and the combinator layer knows how to merge and present that want.

## Soft versus Committed Failures

The `ParseFailureKind` enumeration is the mechanism that prevents a parser combinator from accepting a partial match as a full success. A `Soft` failure at the head of a sequence has not consumed any input that a subsequent alternative would need to re-consume; the `|` operator may safely rewind and try the next branch. A `Committed` failure indicates that the parser advanced the cursor past a point of no return, typically because a sub-parser in a sequence succeeded before a later sub-parser failed.

The `&` (sequence) operator encodes the rule directly. When the second operand fails after the first operand has consumed input, the sequence promotes the failure to `Committed` so that an enclosing alternative does not retry from the original position. The relevant fragment, quoted from the header, is:

```cpp
auto rb = b (in);
if (!rb)
  {
    bool consumed = in.pos != save;
    in.pos = save;
    auto kind = rb.error.kind;
    if (consumed)
      kind = ParseFailureKind::Committed;
    return ExpectedResult<std::pair<A, B>>::failure (
        rb.error.pos, kind, rb.error.expected);
  }
```

The `|` (choice) operator honours this classification. If the first alternative fails with `Committed`, choice does not attempt the second alternative at all; the committed failure is propagated unchanged. Only a `Soft` failure permits the alternative to rewind and try again. This is the standard "cut" discipline found in parser-combinator libraries, and it is the reason a grammar can express "once you have seen this token, you are committed to this branch."

The `try_parse` helper exists to downgrade a `Committed` failure back to `Soft` while rewinding the cursor, for the rare cases where a branch that consumed input should still be retried. The `optional` combinator similarly converts a `Soft` failure into an empty `std::optional<T>` value while propagating a `Committed` failure unchanged. A grammar author who understands these two classifications can predict precisely which alternatives will be attempted.

## Accumulating the "Expected" Set with `merge_error`

A useful diagnostic does not merely report where the parse failed; it reports what the parser expected at that position. When a choice fails on every branch, each branch contributes its own label. The `merge_error` function combines two `ParseError` values into a single representative diagnostic, preserving the labels from every alternative that failed at the farthest position reached.

The function is quoted verbatim below.

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

The rule is positional. A failure that reached farther than any previous failure replaces the accumulator entirely, discarding the labels of the shorter failures. A failure that reached less far is ignored. A failure at the same position contributes its labels to the accumulator and may promote the kind to `Committed`. The result is that after a choice of `a | b | c` fails, the carried `ParseError` describes the farthest position at which any branch gave up, together with the union of the labels that the branches expected there.

The `|` operator uses `merge_error` to fold the errors of its two alternatives. A chain `a | b | c` is therefore right-associative in error accumulation: the failures of `a` and `b` are merged into the failure of `c`, and the resulting `expected` vector lists every label that any branch would have accepted at the farthest position. This is what produces diagnostics such as "expected digit, identifier, or `)`".

The `*` (kleene) operator also uses `merge_error`. Each iteration that fails updates the accumulator, and a `Soft` failure terminates the loop without being reported as an error. A `Committed` failure, by contrast, propagates immediately, because the kleene operator cannot recover from a branch that has already committed.

## The Top-Level Entry Point: `run_parser`

A combinator tree is a `Parser<T>`; it does not run itself. The `run_parser` function is the conventional entry point that takes a parser and a source string and produces a `ParseOutcome<T>`. It seeds a `ParsecInput` from the source, runs the parser, and crucially checks that all input was consumed before reporting success.

The definition, quoted verbatim, is short enough to inspect in full.

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

The return type `ParseOutcome<T>` mirrors `ExpectedResult<T>` but drops the `std::optional` indirection on the error path: a `ParseOutcome` always carries a `ParseError`, even on success, where it is default-constructed (position zero, soft, empty expected). The caller tests `out.value.has_value()` to distinguish success from failure and reads `out.error` for the diagnostic.

The two failure modes are distinct. If the parser itself fails, `run_parser` returns the parser's `ParseError` unchanged. If the parser succeeds but the cursor has not reached the end of the source, `run_parser` synthesises a `Committed` `ParseError` at the current position with the label `<end-of-input>`. The label is literal; it is the string the formatter will render when describing what was expected.

The choice to make trailing input a `Committed` failure is deliberate. A grammar that recognises a prefix of the input and silently ignores the rest is rarely what the caller wants; the most common parser bug is a partial-but-silent success. By turning trailing input into a hard failure, `run_parser` forces the grammar author either to consume the rest explicitly or to acknowledge that the grammar is intentionally prefix-only.

## End-of-Input Handling and the Trailing-Input Diagnostic

The trailing-input check is the single most important safeguard in `run_parser`. Without it, a parser that matches the first token of a long input would report success and discard the remainder. The check uses `in.eof()`, which compares `pos` against `source.size()`, and reports the position of the first unconsumed character.

Consider a grammar that parses a single integer and is fed the string `"42abc"`. The integer parser matches `42` and stops; the cursor sits at the `a`. Without the end-of-input check, the caller would receive the integer `42` and never learn that `abc` was ignored. With the check, the caller receives a `ParseError` at position 2 with the label `<end-of-input>`, which is exactly the diagnostic that points to the stray suffix.

The label `<end-of-input>` is reserved by convention. A grammar author who writes a primitive that explicitly matches the end of input should use the same label so that diagnostics remain consistent. There is no built-in `eof` parser in the core layer, but one is straightforward to define using the `parser` constructor and `fail_expected`.

```cpp
inline dsl::Parser<char>
end_of_input()
{
  return dsl::labeled(
      dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<char> {
        if (in.eof())
          return '\0';
        return dsl::fail_expected<char>(in, "<end-of-input>");
      }),
      "<end-of-input>");
}
```

When a grammar composes `p & end_of_input()` explicitly, the trailing-input check in `run_parser` becomes redundant but harmless, because the grammar itself will fail at the end-of-input parser with the same label. The two mechanisms agree by construction.

A grammar that is intentionally prefix-only should not use `run_parser`. Instead, the caller should seed a `ParsecInput` directly and invoke the parser, inspecting the resulting `ExpectedResult` and the final `in.pos` as appropriate. This pattern is used internally by `parse_with_action`, which never checks end-of-input because it is a building block rather than a top-level driver.

## Turning a `ParseError` into a Human-Readable Message

The `ParseError` structure is intentionally minimal: a byte offset, a kind, and a list of labels. Converting it into a message suitable for an end user is the caller's responsibility, because the appropriate format depends on the host application. A REPL wants a one-line message; a compiler wants a multi-line excerpt with a caret.

The first conversion is from byte offset to line and column. This is a linear scan over the source string up to the failure position, counting newline characters. The line is one-based; the column is the number of characters since the last newline, also one-based. The scan is cheap enough for interactive use and avoids the need to maintain a separate line table.

```cpp
struct SourcePos { std::size_t line; std::size_t col; };

SourcePos line_col(std::string_view src, std::size_t offset)
{
  SourcePos p{1, 1};
  for (std::size_t i = 0; i < offset && i < src.size(); ++i)
    {
      if (src[i] == '\n')
        {
          ++p.line;
          p.col = 1;
        }
      else
        {
          ++p.col;
        }
    }
  return p;
}
```

The second conversion is from the `expected` vector to a phrase. When the vector has one element, the phrase is "expected X". When it has several, the elements are joined with commas and a final "or", as in "expected digit, identifier, or `)`". An empty vector indicates that the parser failed without specifying what it wanted, which is usually a sign that a primitive was constructed without a label.

```cpp
std::string format_expected(const std::vector<std::string> &expected)
{
  if (expected.empty())
    return "unexpected input";
  if (expected.size() == 1)
    return "expected " + expected.front();
  std::string out = "expected ";
  for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (i + 1 == expected.size())
        out += "or " + expected[i];
      else if (i + 2 == expected.size())
        out += expected[i] + " ";
      else
        out += expected[i] + ", ";
    }
  return out;
}
```

Combining the two gives a complete formatter. The failure kind is rarely surfaced to the end user, who does not care whether a failure was soft or committed, but it is useful in debug output for the grammar author. A production formatter might render the source line containing the failure position with a caret beneath the offending column.

```cpp
std::string format_error(std::string_view src, const dsl::ParseError &err)
{
  auto pos = line_col(src, err.pos);
  return std::to_string(pos.line) + ":" + std::to_string(pos.col) + ": " +
         format_expected(err.expected);
}
```

With this formatter, the diagnostic from `run_parser(p, "xz")` against the grammar `labeled(ch('x') & ch('y'), "xy")` becomes `1:2: expected y`. The position points at the `z`, which is where the second character of the sequence failed to match, and the label comes from the `ch('y')` primitive that the `labeled` wrapper would have overwritten only on failure of the whole sequence.

## A Complete Diagnostic Example

The diagnostic example shipped with the library, reproduced below, exercises the full path from `labeled` through `run_parser` to a printed position.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto p = dsl::labeled(dsl::ch('x') & dsl::ch('y'), "xy");
  auto out = dsl::run_parser(p, "xz");
  if (!out.value) {
    std::cout << "error at " << out.error.pos << "\n";
  }
}
```

The grammar expects the literal two-character sequence `xy`. The input `xz` matches the first character, fails on the second, and the `&` operator promotes the failure to `Committed` because the first operand consumed input. The `labeled` wrapper overwrites the `expected` vector with the single label `xy` on failure, so the resulting `ParseError` carries position 1 and the label `xy`.

Tracing the flow illustrates the interaction of the components. The `ch('x')` primitive peeks at position 0, sees `x`, and consumes it; the cursor advances to 1. The `ch('y')` primitive peeks at position 1, sees `z`, and returns a `Soft` failure with label `y`. The `&` operator observes that the cursor moved (from 0 to 1), promotes the kind to `Committed`, rewinds the cursor to 0, and returns the failure. The `labeled` wrapper overwrites `expected` with `xy`. `run_parser` receives the failure and returns it as the `ParseOutcome`'s error field.

The printed message `error at 1` is the byte offset. A more polished driver would convert this to `1:2` using the `line_col` helper above, producing the familiar compiler-style coordinate. The library deliberately stops short of prescribing a format, leaving the rendering to the embedding application.

## Semantic Actions: `parse_with_action`

Recognition is half the work of a parser; the other half is turning the recognised text into a structure. DSLtk provides the `parse_with_action` template for the case where a grammar author wants a callback to fire whenever a particular parser matches, receiving the matched span and an external semantics map. The callback may return a value, which `parse_with_action` forwards to its caller as an `std::optional`.

The definition is quoted verbatim below.

```cpp
/**
 * @brief Parser stage metadata passed to semantic hooks.
 */
struct ParserStage
{
  std::size_t begin{};
  std::size_t end{};
  std::string_view span{};
};

/**
 * @brief Runs a parser and invokes semantic action with matched span.
 */
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

Three points deserve emphasis. First, the action fires only on success: the `if (!r) return std::nullopt;` guard ensures that a failed parse produces no side effect through the action. Second, the matched span is computed from the cursor positions before and after the parse, using `ParsecInput::get_span`, so the action receives the exact text that the parser consumed regardless of what the parser returned as its value. Third, the `semantics` map is passed through unchanged; it is a slot for the host application to thread external context, such as a table of production names to their currently bound values.

The `ParserStage` carries `begin`, `end`, and `span`. The `span` is a `string_view` into the source, so it is cheap to construct and cheap to pass. The `begin` and `end` fields are absolute byte offsets, which the action may use to construct a source-range annotation on an AST node.

The return type is deduced from the action's return type via `decltype(action(semantics, ParserStage{}))`. An action that returns `void` is not supported directly because `std::optional<void>` is not a valid type; an action intended for its side effects should return a sentinel such as `bool` or `int`. The deduction is the reason the signature is written in trailing-return-type form.

## Attaching an Action to a Primitive

The simplest use of `parse_with_action` attaches an action to a primitive parser and inspects the matched span. The following example parses an integer literal and stores the matched text in a vector for later use.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <vector>
#include <string>

int main() {
  auto digit = dsl::satisfy(
      [](char c) { return c >= '0' && c <= '9'; }, "digit");
  auto number = *digit;

  std::vector<std::string> matched;
  dsl::ParsecInput in{"42", 0};
  auto r = dsl::parse_with_action(
      number, in, {},
      [&](const std::unordered_map<std::string, std::string> &,
          const dsl::ParserStage &stage) {
        matched.emplace_back(stage.span);
        return 0;
      });
  if (r)
    std::cout << "matched " << matched.front() << "\n";
}
```

The action receives the span `"42"` and pushes it into the external vector. The return value `0` is a dummy that satisfies the `std::optional<int>` return type. Note that `parse_with_action` does not check end-of-input; it is a building block, and the caller is responsible for any trailing-input check.

The same pattern works for any parser, including a sequence. Wrapping `ch('x') & ch('y')` in `parse_with_action` delivers the span `"xy"` to the action when both characters match, and delivers nothing when either fails. The action is thus a clean way to observe the text consumed by an arbitrary sub-grammar without threading callbacks through every primitive.

## Building a Small AST: Parsing `key=value`

A more realistic example parses a `key=value` pair and constructs a small AST from the matched spans. The AST node type is sketched here using the `dsl::ASTNode` value type introduced in Chapter 10; the principle is the same for any node representation the host application prefers.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

struct KvNode {
  std::string key;
  std::string value;
};

int main() {
  auto letter = dsl::satisfy(
      [](char c) { return (c >= 'a' && c <= 'z'); }, "letter");
  auto digit = dsl::satisfy(
      [](char c) { return c >= '0' && c <= '9'; }, "digit");
  auto ident = *letter;
  auto val = *digit;

  auto eq = dsl::ch('=');

  std::vector<KvNode> ast;
  auto parse_pair = [&](dsl::ParsecInput &in) -> dsl::ExpectedResult<int> {
    std::string k, v;
    auto rk = dsl::parse_with_action(
        ident, in, {},
        [&](const std::unordered_map<std::string, std::string> &,
            const dsl::ParserStage &s) {
          k = std::string(s.span);
          return 0;
        });
    if (!rk) return dsl::ExpectedResult<int>::failure(in.pos);
    auto re = eq(in);
    if (!re) return dsl::ExpectedResult<int>::failure(re.error.pos);
    auto rv = dsl::parse_with_action(
        val, in, {},
        [&](const std::unordered_map<std::string, std::string> &,
            const dsl::ParserStage &s) {
          v = std::string(s.span);
          return 0;
        });
    if (!rv) return dsl::ExpectedResult<int>::failure(rv.error.pos);
    ast.push_back({k, v});
    return 0;
  };

  auto pair_parser = dsl::parser(parse_pair);
  auto out = dsl::run_parser(pair_parser, "count=42");
  if (out.value)
    std::cout << "ast: " << ast.front().key << "=" << ast.front().value << "\n";
}
```

The lambda captures the `ast` vector by reference and pushes a node whenever both the key and the value match. The `parse_with_action` calls deliver the matched spans without the surrounding orchestration needing to know how `ident` and `val` are implemented. The `eq` parser is invoked directly because its matched character is not needed.

This pattern composes well. A larger grammar can wrap each production in a similar lambda, pushing nodes into a shared AST vector as each production succeeds. The result is a hand-written recursive-descent translator that uses `dsl`'s combinators for recognition and the action hook for construction, with no need to thread a visitor over the parser's return type.

## Accumulating Results Across a List

The `*` (kleene) combinator returns a `std::vector<T>` of the values produced by each iteration. When the per-iteration value is unimportant and the matched text is what matters, `parse_with_action` can be wrapped inside the iterated parser so that each match appends to an external container. The following example parses a comma-separated list of integers and accumulates their numeric values.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>

int main() {
  auto digit = dsl::satisfy(
      [](char c) { return c >= '0' && c <= '9'; }, "digit");
  auto comma = dsl::ch(',');

  std::vector<long> values;
  auto parse_int = dsl::parser([&](dsl::ParsecInput &in)
                                   -> dsl::ExpectedResult<int> {
    auto r = dsl::parse_with_action(
        *digit, in, {},
        [&](const std::unordered_map<std::string, std::string> &,
            const dsl::ParserStage &s) {
          values.push_back(std::stol(std::string(s.span)));
          return 0;
        });
    if (!r) return dsl::ExpectedResult<int>::failure(in.pos);
    return 0;
  });

  auto elem = parse_int & optional(comma);
  auto list = *elem;
  auto out = dsl::run_parser(list, "1,2,3");
  if (out.value) {
    for (long v : values) std::cout << v << " ";
    std::cout << "\n";
  }
}
```

Each successful match of `*digit` fires the action, which parses the span with `std::stol` and appends the result. The `optional(comma)` allows a trailing comma without forcing one. The outer `*elem` iterates until the input is exhausted, and `run_parser` enforces end-of-input so that a stray non-digit suffix is reported as an error rather than silently truncating the list.

The example illustrates a trade-off. The kleene combinator's return value (a `std::vector<std::pair<int, std::optional<char>>>`) is discarded in favour of the side effect through the action. This is appropriate when the matched text is the only datum the host application needs; when the structured return value is also useful, the action can augment rather than replace it.

## Productions and the `ProductionHandler` Carrier

For grammars that name their productions at compile time, the header provides the `ProductionHandler` template and the `production<Id>` factory. These do not run a parser by themselves; they are carriers that bind a compile-time `FixedString` identifier (Chapter 4) to a handler callable, intended for use by table-driven grammar layers such as the PEG machinery of Chapters 27 and 28.

The definitions are quoted verbatim.

```cpp
template <FixedString Id, typename Fn> struct ProductionHandler
{
  Fn fn;
};

template <FixedString Id, typename Fn>
auto
production (Fn &&fn)
{
  return ProductionHandler<Id, std::decay_t<Fn>>{ std::forward<Fn> (fn) };
}
```

A `ProductionHandler` is a value type that carries a `FixedString` key in its template parameter list and a callable in its single field. The `production<"Expr">(fn)` form returns a `ProductionHandler<"Expr", std::decay_t<Fn>>` that the host application can store in a grammar table. The PEG layer (Chapter 27) uses these handlers to dispatch on the production identifier when a rule matches, threading the matched span into the handler in the same spirit as `parse_with_action`.

The handler's signature is not constrained by the carrier; the callable may take whatever arguments the table-driven layer passes. This makes `ProductionHandler` a uniform vehicle for semantic actions across the PEG layer, where the identifier is known at compile time but the matched text is only known at run time.

The relationship between `parse_with_action` and `ProductionHandler` is one of layering. The former is a direct, unkeyed hook for hand-written grammars; the latter is a keyed, table-friendly hook for generated or declarative grammars. Both deliver the same information — the fact of a match and the span it consumed — to a user-supplied callback. A grammar author chooses between them based on whether the grammar is written as a combinator tree or as a table of named productions.

## Reporting a Friendly Error on Malformed Input

Combining `run_parser` with a formatter yields a complete driver that reports a friendly message on failure. The following example parses an integer with an optional sign and reports either the parsed value or a coordinate-and-expected diagnostic.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>
#include <unordered_map>

struct SourcePos { std::size_t line; std::size_t col; };
SourcePos line_col(std::string_view src, std::size_t off) {
  SourcePos p{1, 1};
  for (std::size_t i = 0; i < off && i < src.size(); ++i)
    (src[i] == '\n') ? (++p.line, p.col = 1) : ++p.col;
  return p;
}

int main() {
  auto sign = dsl::optional(dsl::ch('-'));
  auto digit = dsl::satisfy(
      [](char c) { return c >= '0' && c <= '9'; }, "digit");
  auto integer = sign & *digit;

  std::string src = "-4x";
  auto out = dsl::run_parser(integer, src);
  if (!out.value) {
    auto p = line_col(src, out.error.pos);
    std::cout << src << ":" << p.line << ":" << p.col << ": ";
    if (!out.error.expected.empty())
      std::cout << "expected " << out.error.expected.front();
    std::cout << "\n";
  } else {
    std::cout << "parsed\n";
  }
}
```

The input `-4x` matches the sign and one digit, then fails to match a second digit at the `x`. The `*digit` kleene terminates softly at position 2, the sequence succeeds with the matched value, and `run_parser`'s end-of-input check then fires at position 2 with the label `<end-of-input>`. The formatter renders this as `-4x:1:3: expected <end-of-input>`, which points the user at the stray `x`.

This diagnostic is more informative than a bare "parse failed" because it identifies both the location and the expectation. The label `<end-of-input>` is the cue that the parser succeeded on its own terms but left input unconsumed, which is a different class of error from a primitive failing to match.

A grammar that wishes to report a more specific label at the trailing position can compose an explicit `end_of_input()` parser, as shown earlier, in place of relying on `run_parser`'s check. The two approaches produce the same label and the same position; the explicit composition is preferable when the grammar is intended for reuse outside `run_parser`.

## How Actions Compose Within a Larger Grammar

An action attached to a sub-parser is local to that sub-parser. When the sub-parser is embedded in a sequence or a choice, the action fires exactly when the sub-parser matches, and the surrounding combinator is unaware that an action was involved. This locality is what makes `parse_with_action` usable as a building block: the host application wraps the sub-parsers that need side effects and leaves the rest alone.

Inside a sequence `a & b`, an action on `a` fires before `b` is attempted. If `b` subsequently fails, the action has already fired and its side effect persists. This is rarely a problem because the surrounding grammar typically rewinds and retries only on `Soft` failures, and a `Soft` failure of `b` after a successful `a` is promoted to `Committed` by the sequence operator, preventing a retry. A grammar author who is concerned about a partially-applied side effect can structure the grammar so that the action is attached to the whole sequence rather than to a prefix.

Inside a choice `a | b`, an action on `a` fires only when `a` succeeds, because the `|` operator returns `a`'s result immediately on success and never reaches `b`. An action on `b` fires only when `a` fails softly and `b` succeeds. There is no scenario in which both actions fire for the same input, because the `|` operator stops at the first successful alternative.

Inside a kleene `*a`, an action on `a` fires once per successful iteration. A `Soft` failure of `a` terminates the loop without firing the action, which is the desired behaviour: the action observes only the matches that actually occurred. A `Committed` failure of `a` propagates immediately, again without firing the action for the failing iteration.

## Pitfalls and Common Mistakes

The first pitfall is forgetting to check end-of-input. A grammar run directly through `p(in)` rather than through `run_parser` will report success on a prefix match and silently discard the rest. The remedy is either to use `run_parser` at the top level or to compose an explicit `end_of_input()` parser into the grammar.

The second pitfall is attaching an action to a prefix of a sequence that later fails. The action fires on the prefix's success and its side effect persists even though the sequence as a whole fails. The remedy is to attach the action to the whole sequence, or to buffer the side effect in the action and commit it only when the enclosing production succeeds.

The third pitfall is misreading the `span` passed to an action. The span is the text between the cursor positions before and after the parse, which is the text the parser consumed. A parser that internally rewinds (such as `try_parse`) will report a span that does not include the rewound characters. A parser that does not advance the cursor on success (which is unusual but possible for a custom primitive) will report an empty span.

The fourth pitfall is assuming that an empty `expected` vector means "no diagnostic". An empty vector means the failing primitive did not label itself, which is usually a bug in the grammar. The `labeled` wrapper and the `satisfy`/`ch` primitives all attach labels; a custom primitive constructed via `parser(...)` that returns `ExpectedResult::failure` without an expected list will produce a diagnostic with no expected set, and the formatter will render it as "unexpected input".

The fifth pitfall is confusing `Soft` and `Committed` when reasoning about alternatives. A `Committed` failure short-circuits the `|` operator, so a grammar that expects backtracking must ensure that its sub-parsers fail softly. The `try_parse` wrapper downgrades a `Committed` failure to `Soft` for the rare case where backtracking past a consumed prefix is intentional.

The sixth pitfall is expecting `parse_with_action` to check end-of-input. It does not; it is a building block. The end-of-input check is the responsibility of `run_parser` or of an explicit `end_of_input()` parser in the grammar.

## Relationship to the Result Type and Error Flow

The `ExpectedResult<T>` and `ParseOutcome<T>` types are the parser-layer instances of the general error-flow pattern introduced in Chapter 22. The `Result<T, E>` type discussed there is the canonical sum type for operations that may fail; `ExpectedResult` specialises this pattern for parsers by carrying the `ParseError` as the error component and by providing the `failure` factory that constructs a failing result in one call.

The `load_parse_input_file` helper, which returns a `Result<std::string, std::string>`, is the bridge between file I/O and the parser layer. A driver that reads a source file uses `load_parse_input_file` to obtain the source text, threads the text into `run_parser`, and then formats the resulting `ParseError` for the user. The two error types — the `std::string` of `Result` and the `ParseError` of `ExpectedResult` — serve different layers and are not interchanged.

A production driver may unify the two by wrapping `run_parser`'s `ParseOutcome` in a `Result<T, ParseError>`. This is straightforward because `ParseOutcome` already separates the value from the error. The benefit is that the driver's error path can be handled with the same `match`/`when`/`otherwise` machinery (Chapter 8) that the rest of the application uses for its `Result`-returning operations.

## Forward References

The PEG layer of Chapters 27 and 28 builds directly on the material here. The `PEGMatch` type used by the PEG combinators carries a `value` field that is set to the matched span in exactly the way that `parse_with_action` delivers a span to its action, and the PEG `add_rule` machinery uses `ProductionHandler` to dispatch on compile-time production identifiers. A reader who understands `parse_with_action` and `ProductionHandler` will recognise the same patterns in the PEG layer, scaled up to a full grammar definition language.

The AST machinery of Chapter 10 is the natural destination for the spans that actions deliver. A `ParserStage::span` becomes the text of a leaf node or the source range of an interior node; the `leaf<>` and `node<>` builders of Chapter 11 accept these as construction arguments. A complete translator, then, is a grammar of `dsl` combinators whose actions build `dsl::ASTNode` trees, run through `run_parser`, with diagnostics formatted by the helpers shown above.

The task-pipeline layer of Chapter 26 is the next stop for readers who want to drive a parser as part of a larger processing chain. A `Task` can wrap a `run_parser` call and store its `ParseOutcome` in the shared `TaskState`, allowing a parser to be composed with tokenisers, rewrite passes, and code generators in a uniform pipeline.

## Summary

`dsl`'s parser layer reports failure through `ParseError`, a triple of byte position, soft-or-committed kind, and a vector of expected labels. The `merge_error` function accumulates labels across alternatives at the farthest position reached, so that a failed choice reports everything the grammar would have accepted at the point of failure. The `Soft`/`Committed` distinction governs backtracking: a `Committed` failure short-circuits choice, while a `Soft` failure permits the next alternative to be tried.

`run_parser` is the top-level entry point that seeds a `ParsecInput`, runs a parser, and refuses to report success when trailing input remains. The trailing-input check is a `Committed` failure labelled `<end-of-input>`, and it is the primary defence against the partial-but-silent-success bug. Converting a `ParseError` to a human-readable message is the caller's responsibility; a linear scan over the source yields line and column, and a join over the `expected` vector yields the "expected X" phrase.

`parse_with_action` attaches a callback to a parser that fires on success, receiving a `ParserStage` with the matched span. The action composes locally within sequences, choices, and kleene iteration, firing exactly when its sub-parser matches. The `ProductionHandler` carrier and the `production<Id>` factory generalise the same idea to table-driven grammars with compile-time production identifiers. Together, these facilities turn a recogniser into a translator, bridging the combinator layer to the AST machinery of Chapter 10 and the PEG layer of Chapters 27 and 28.
