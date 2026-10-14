# Chapter 27: The PEG Definition Grammar: Rules and Channels

The PEG (Parsing Expression Grammar) engine is a lightweight, additive grammar subsystem layered on top of the pattern-matching and parser-combinator machinery introduced earlier in this manual. Where Chapter 9 described the compile-time `dsl::pattern<>` regex engine, and Chapters 23 through 25 built parsers from small composable functions, the PEG engine provides a container-oriented model: a grammar is a set of named rules, each carrying a compile-time pattern and a semantic action, evaluated under PEG ordered-choice semantics. This chapter describes the grammar container and its rules. The matcher machinery that consumes a grammar streamingly is the subject of Chapter 28.

The PEG subsystem is deliberately additive. No existing type, function, or layout in `DSLtk.hpp` is modified to accommodate it; the entire engine lives in a single self-contained section of the header behind forward-declared structs (`PEGDefinition`, `PEGRule`, `PEGMatch`, `PEGParseResult`, `PEGMatcher`). A grammar can be constructed, populated with rules, and used to parse input without pulling in any of the combinator types from Chapter 23. Readers familiar with the `dsl::pattern` regex subset will find the pattern language identical, because the PEG engine compiles its rule patterns with the same runtime matcher described in Chapter 9.

This chapter covers the factory `create_peg_definition()`, the `PEGDefinition` container, the `add_rule<Pattern>(action)` template, the `PEGRule` record, the channel abstraction (`PEGChannel`, `PEGIgnoreChannel`, `PEGDefaultChannel`, `new_peg_channel`), the `PEGMatch` value handed to semantic actions, the `PEGParseResult` returned by `parse()`, and the ordered-choice algorithm that drives rule selection. Worked examples progress from a trivial whitespace-skipping tokenizer to a small expression grammar. Cross-references to Chapter 4 (FixedString), Chapter 9 (pattern subset), Chapter 10 (ASTNode), and Chapters 23 through 25 (parser combinators) appear throughout.

## The PEG Model in DSLtk

A Parsing Expression Grammar, in the formal sense, is an ordered set of production rules where each rule is a parsing expression and choice is ordered: the first alternative that matches wins, and later alternatives are not tried at the same position. The DSLtk PEG engine embodies that semantics directly. A `PEGDefinition` stores rules in declaration order, and `parse()` walks the input position by position, attempting each visible rule at the current offset until one succeeds. There is no backtracking across positions, no ambiguity, and no lookahead beyond what the pattern subset itself provides.

This makes the engine a scanner rather than a recursive-descent parser of nested structures. Each rule consumes a contiguous span of input at the current position, fires its semantic action, and advances the position. The grammar does not, on its own, build a recursive tree; that is the job of the semantic action, which may use the AST facilities of Chapter 10 and Chapter 11 to construct a tree, or simply emit tokens. The PEG engine's responsibility is narrower: decide which rule matches here, run its action, advance.

Because the engine is layered on the `dsl::pattern` regex subset, every rule pattern is a flat sequence of atoms and quantifiers. There is no alternation operator `|`, no parenthesised grouping, and no backreferences. Composition of alternatives is expressed at the rule level, by declaring several rules in order, not within a single pattern string. This restriction is a deliberate consequence of reusing the Chapter 9 matcher and keeps rule patterns simple to read and cheap to compile.

## Creating a Grammar

A grammar is created with the free function `create_peg_definition()`, which returns a default-constructed `PEGDefinition` by value. The definition owns its rules in an internal `std::vector<PEGRule>` and returns stable references to them as long as rules are only appended and the definition itself remains alive. Moving a `PEGDefinition` is supported by the implicit move constructor of its members, but references obtained before a move must not be used after the move.

```cpp
auto grammar = dsl::create_peg_definition();
```

The returned object has its `parent` pointer set to `nullptr`, marking it as a root grammar. Root grammars are the usual starting point; derived grammars, created with `derive_peg_definition(parent)`, are discussed later in this chapter. From the moment of creation the grammar contains no rules, and a call to `parse()` on an empty grammar will simply fail to match the first character.

The factory is intentionally trivial. It exists so that user code does not need to name the `PEGDefinition` constructor or worry about default member values; it also makes the intent at the call site explicit. The grammar is a value type, and a typical program will keep one in an automatic variable for the duration of parsing.

## The PEGDefinition Container

`PEGDefinition` is the grammar container. Its public surface consists of the `parent` pointer, the `add_rule` template, the `rules()` accessor, and the `parse()` entry point. The container itself holds rules by value, compiles each rule's pattern on insertion, and assigns each rule a monotonically increasing `order` field that records declaration precedence.

The `rules()` member returns a `std::vector<const PEGRule*>` covering all rules visible at this level. For a root grammar, that is exactly the locally appended rules in insertion order. For a derived grammar, parent rules come first, followed by local rules, preserving the PEG ordered-choice precedence that parent behaviour is consulted before child overrides. This flattened view is what `parse()` iterates over.

```cpp
std::vector<const PEGRule *> visible = grammar.rules();
```

The container does not expose mutation of existing rules beyond the references returned by `add_rule`. Because `add_rule` returns a `PEGRule&`, callers may configure the rule after insertion: setting its channel, toggling `must_fail`, or attaching an `if_succeed` follow-up rule. These post-insertion mutations are how advanced behaviour is expressed, and they are documented in the second half of this chapter.

## Adding Rules with add_rule

Rules are added through the `add_rule` template, which takes a `FixedString` non-type template parameter (Chapter 4) for the pattern and a callable for the semantic action. The pattern is therefore a compile-time string literal, which lets the engine verify the pattern at compile time and avoid runtime parsing of the pattern text on every call. The action is wrapped in a `std::function<void(PEGMatch&)>` and stored inside the rule.

The two overloads, quoted verbatim from the header, are:

```cpp
template <FixedString S, typename Fn>
PEGRule &
add_rule (Fn &&action)
{
  return emplace_rule (S.view (),
                       PEGSemanticAction (std::forward<Fn> (action)));
}

template <FixedString S>
PEGRule &
add_rule ()
{
  return emplace_rule (S.view (), PEGSemanticAction{});
}
```

The first overload is the common form: a pattern and a callback. The second overload adds a rule with an empty action, useful when the rule is intended only to advance the position or when the action will be attached later through the returned reference. Both overloads delegate to a private `emplace_rule` that compiles the pattern, throws `std::invalid_argument` on a malformed pattern, and stores the rule.

A typical call site binds the returned reference to a name so that the rule can be configured afterwards. The pattern string uses the Chapter 9 regex subset: literals, the `.` wildcard, character classes `[...]` with ranges and `^` negation, and the quantifiers `*`, `+`, `?`, and the negative lookahead `!`. Anchors `^` and `$` are accepted and silently ignored, since PEG matching is implicitly anchored at the current position.

```cpp
auto &identifier = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(
    [](dsl::PEGMatch &m) { std::cout << "id:" << m.value << "\n"; });
```

Because the pattern is a `FixedString` NTTP, escape sequences inside the string literal must be written exactly as they would appear in an ordinary C++ string. The whitespace pattern `"[ \\t\\n]+"` is the canonical example: the backslashes are part of the C++ string literal and produce the characters `\t` and `\n` inside the `FixedString`, which the Chapter 9 matcher then interprets as tab and newline.

## The PEGRule Record

A `PEGRule` is the runtime representation of a single production. Its public members are the compiled pattern, the semantic action, the channel, and three advanced fields governing negated and follow-up matching. The complete layout, quoted from the header, is:

```cpp
struct PEGRule
{
  std::string pattern_text{};
  peg_detail::CompiledPattern compiled{};
  PEGSemanticAction semantic_action{};
  PEGChannel channel{ "@DEFAULT" };
  bool must_fail{ false };    ///< succeed only when the pattern fails
  bool fire_on_neg{ false };  ///< run the action even on negated success
  PEGRule *if_succeed{ nullptr }; ///< rule to attempt when pattern matches
  std::size_t order{ 0 };     ///< declaration order (PEG precedence)
  // ...
};
```

`pattern_text` retains the original pattern string for diagnostics. `compiled` holds the runtime-compiled `CompiledPattern` produced by the Chapter 9 compiler; it is the object actually consulted during matching. `semantic_action` is the user-supplied callback. `channel` is the routing channel, discussed in the next section. The remaining fields — `must_fail`, `fire_on_neg`, `if_succeed` — control advanced behaviour that is outside the main flow of this chapter but is documented here for completeness.

The `order` field is assigned by `emplace_rule` from a monotonically increasing counter on the owning definition. It is preserved for diagnostic use and to stabilise sorting; `parse()` does not consult `order` directly because the flattened `rules()` vector already encodes precedence by position. Callers should not rely on specific numeric values of `order`, only on its monotonicity within a definition.

`PEGRule` is movable but not copyable through the usual vector semantics; it is stored by value in the definition's internal vector. References returned by `add_rule` remain valid while the vector's storage is stable, which holds as long as rules are only appended and the definition is not moved. Programs that store rule references across moves of the definition should re-obtain them after the move.

## The try_match Member

The matching core of a rule is `try_match`, which takes the input text and a start offset and returns `std::optional<std::size_t>`: the end offset on success, or `std::nullopt` on failure. The implementation honours `must_fail`: when a rule is configured to fail-succeed, `try_match` returns success exactly when the underlying pattern does not match, consuming one character so that the tokenizer always makes progress.

```cpp
std::optional<std::size_t>
try_match (std::string_view text, std::size_t pos) const
{
  if (must_fail)
    {
      if (compiled.match_at (text, pos))
        return std::nullopt;
      return pos < text.size () ? std::optional<std::size_t> (pos + 1)
                                : std::nullopt;
    }
  return compiled.match_at (text, pos);
}
```

For ordinary (non-negated) rules, `try_match` is a thin forwarder to `CompiledPattern::match_at`, which performs a greedy prefix match. Greedy quantifiers (`*`, `+`, `?`) consume as much input as they can; the `!` quantifier is a zero-width negative lookahead that succeeds without consuming when its atom does not match. A pattern fails as soon as any atom cannot be satisfied at the current position.

Callers normally do not invoke `try_match` directly; `PEGDefinition::parse()` and the `PEGMatcher` of Chapter 28 are the intended entry points. The member is public so that advanced users can probe a rule against arbitrary positions, for example when integrating the PEG engine with a hand-written driver.

## Channels: Routing Matches

Channels are the PEG engine's mechanism for categorising or suppressing matches without altering the grammar. A channel is a `PEGChannel` value: a thin wrapper around a `std::string` name with equality comparison and an `is_ignore()` predicate. The header predeclares two built-in channels and provides a factory for custom ones.

```cpp
struct PEGChannel
{
  std::string name{ "@DEFAULT" };
  constexpr bool operator== (const PEGChannel &) const = default;
  bool is_ignore () const noexcept { return name == "@IGNORE"; }
};

inline const PEGChannel PEGIgnoreChannel{ "@IGNORE" };
inline const PEGChannel PEGDefaultChannel{ "@DEFAULT" };
```

`PEGDefaultChannel` (named `@DEFAULT`) is the channel assigned to every newly created rule. Matches on the default channel are ordinary semantic tokens: they are reported through the `PEGMatch` passed to the action, and they are visible to the matcher arms described in Chapter 28. `PEGIgnoreChannel` (named `@IGNORE`) marks a rule as ignorable. The `PEGMatch::ignored` flag is set to `true` for such matches, and the matcher of Chapter 28 skips them silently before consulting any arm.

Assigning a rule to the ignore channel is a one-line mutation of the returned reference. This is the canonical way to handle whitespace and comments: declare a rule whose pattern matches the ignorable text, give it an action that does nothing (or does bookkeeping only), and route it to `@IGNORE`. The grammar will still consume the text and advance the position, but downstream consumers will not see it as a token.

```cpp
auto &ws = grammar.add_rule<"[ \\t\\n]+">(
    [](dsl::PEGMatch &m) { /* no-op */ });
ws.channel = dsl::PEGIgnoreChannel;
```

The decision to route a match to a channel is a property of the rule, not of the match. Two matches produced by the same rule always share the rule's channel; there is no per-match override. This keeps the model simple and predictable: the channel is part of the rule's identity, like its pattern and its action.

## Custom Channels

Beyond the two built-ins, the engine supports user-defined channels through `new_peg_channel`. A custom channel is just a `PEGChannel` with a caller-chosen name, subject to the single constraint that the name begins with `@`. The factory enforces this at construction time and throws `std::invalid_argument` otherwise.

```cpp
inline PEGChannel
new_peg_channel (std::string_view name)
{
  if (name.empty () || name.front () != '@')
    throw std::invalid_argument (
        "dsl::new_peg_channel: channel names must begin with '@'");
  return PEGChannel{ std::string (name) };
}
```

Custom channels are useful when a grammar produces several categories of token that consumers wish to filter independently. A compiler front end, for example, might route comments to `@COMMENTS`, string literals to `@STRINGS`, and ordinary tokens to `@DEFAULT`. The matcher of Chapter 28 exposes `only`, `include`, and `exclude` filters that select arms by channel, so categorising at the rule level pays off when consuming the stream.

```cpp
const dsl::PEGChannel CommentsChannel = dsl::new_peg_channel ("@COMMENTS");

auto &line_comment = grammar.add_rule<"//[^\\n]*">(
    [](dsl::PEGMatch &) { /* swallow */ });
line_comment.channel = CommentsChannel;
```

Note that custom channels are not ignorable by default; only `@IGNORE` triggers the `is_ignore()` predicate. A custom channel is visible to consumers unless the consumer explicitly filters it out. Routing whitespace to a custom non-ignore channel would cause it to be reported as an ordinary token, which is rarely what is wanted; for whitespace, use `PEGIgnoreChannel`.

## The PEGMatch Record

When a rule matches, the engine constructs a `PEGMatch` describing the match and passes it by reference to the rule's semantic action. The record carries the matched text as a `std::string_view`, the begin and end offsets in the source, a pointer to the producing rule, the resolved channel, optional user data, and 1-based line and column information.

```cpp
struct PEGMatch
{
  std::string_view value{};   ///< matched text
  std::size_t begin{ 0 };     ///< start offset in source
  std::size_t end{ 0 };       ///< one-past-end offset in source
  PEGRule *rule{ nullptr };   ///< rule that produced this match
  PEGChannel channel{};       ///< resolved channel of the rule
  void *user_data{ nullptr }; ///< optional extension point
  std::size_t line{ 1 };      ///< 1-based line of `begin`
  std::size_t column{ 1 };    ///< 1-based column of `begin`
  bool ignored{ false };      ///< true if routed to @IGNORE
  std::size_t length () const noexcept { return end - begin; }
};
```

The `value` view is a slice into the original input text passed to `parse()`. It is valid only for the duration of the parse; actions that need to retain the matched text must copy it into an owning `std::string`. The `begin` and `end` offsets are absolute positions in the source, and `length()` is provided as a convenience for the common `end - begin` query.

The `rule` pointer lets an action inspect the rule that produced the match, including its pattern text and channel. This is occasionally useful when a single action lambda is shared across several rules via capture: the action can branch on `m.rule->pattern_text` or on `m.rule->order`. The `user_data` field is an opaque extension point left at `nullptr` by the engine; callers may stash a pointer to an external state object there through the rule reference before parsing.

Line and column are computed eagerly by the engine using a helper that walks the input once per match. They are 1-based, with columns counting characters from the start of the line. This makes diagnostic messages from semantic actions directly usable without further computation.

## Semantic Actions

A semantic action is any callable convertible to `std::function<void(PEGMatch&)>`. The action is invoked after a rule matches but before the position is advanced, so it may inspect the surrounding input through the original `text` if it has access to it (typically via capture). The action's return value, if any, is discarded. Actions are stored inside the rule and persist for the lifetime of the definition.

The signature `void(PEGMatch&)` is the contract. An action may record a token into a vector captured by reference, build an AST node using the facilities of Chapter 10 and Chapter 11, emit diagnostics, or do nothing at all. The engine does not interpret the action's effects; it merely calls it. This makes the PEG engine agnostic to the downstream representation, in contrast to the parser combinators of Chapter 23 which thread a parser state through the call.

```cpp
std::vector<std::string> tokens;
auto &identifier = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(
    [&tokens](dsl::PEGMatch &m) { tokens.emplace_back (m.value); });
```

Actions on ignore-channel rules are still invoked. The `m.ignored` flag is set to `true`, which lets a shared action distinguish ignorable matches from ordinary ones. A typical whitespace action does nothing, but it could equally well count lines or record comment text for a documentation tool.

Because actions are `std::function` values, they incur the usual overhead of type erasure and dynamic dispatch. For the grammars the PEG engine is designed for — tokenisers and small scanners — this cost is negligible relative to the cost of pattern matching itself. Callers concerned with overhead may pass stateless lambdas, which are stored efficiently inside `std::function`.

## The parse Entry Point

`PEGDefinition::parse(std::string_view text)` is the entry point that runs the rule set over an input. It returns a `PEGParseResult` summarising the outcome: whether the whole input was consumed, the final offset, and on failure a human-readable message plus the line and column of the failing position.

```cpp
PEGParseResult
parse (std::string_view text) const
{
  PEGParseResult result;
  auto all = rules ();
  std::size_t pos = 0;
  while (pos < text.size ())
    {
      bool matched = false;
      for (const PEGRule *r : all)
        {
          auto end = r->try_match (text, pos);
          if (!end)
            continue;
          // ... fire action, advance pos, break ...
        }
      // ...
    }
  result.offset = pos;
  return result;
}
```

The algorithm is a straightforward ordered-choice loop. At each position, the engine iterates over the flattened rule list and calls `try_match` on each rule. The first rule that returns a non-empty match wins; its action is invoked and the position is advanced to the match end. If no rule matches, the parse fails at the current offset. If a rule matches empty input (a zero-width positive match), the parse fails immediately to avoid an infinite loop.

The `parse` member is `const`, so it may be called on a temporary grammar or on a grammar held by `const` reference. It does not mutate the definition or its rules; semantic actions that need to accumulate state must capture external storage by reference. The constness also means that channels configured before the call are honoured as-is, and actions installed before the call are the ones that fire.

## Ordered Choice in Detail

Ordered choice is the defining semantics of PEG, and the `parse` loop implements it faithfully. The flattened rule list returned by `rules()` is consulted in order: parent rules first, then local rules in declaration order. The first rule whose `try_match` returns a non-empty result is the winner; later rules at the same position are not tried. This is the meaning of the `break` statement that closes the inner loop.

Because choice is ordered, the sequence in which rules are added matters. A more specific rule must precede a more general one, or it will never be reached. The classic case is keywords versus identifiers: a rule for the literal `if` must be declared before the rule for `[a-zA-Z_][a-zA-Z_0-9]*`, otherwise the identifier rule will consume `if` as an identifier and the keyword rule will never see it.

```cpp
auto &kw_if    = grammar.add_rule<"if">   (print_kw);
auto &kw_then  = grammar.add_rule<"then"> (print_kw);
auto &kw_else  = grammar.add_rule<"else"> (print_kw);
auto &ident    = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> (print_id);
```

Ordered choice also means that a broad pattern early in the list can shadow a narrower one later. The engine does not perform longest-match analysis across rules; the first match wins regardless of length. A rule matching `[a-z]+` placed before a rule matching `if` will always consume the `if` prefix, leaving the second rule unreachable. Authors must therefore order rules from most specific to least specific.

This differs from regular-expression alternation, where the longest match typically wins across alternatives. The PEG engine's rule is simpler and cheaper: first match wins. Authors accustomed to regex alternation should be careful to translate alternatives into ordered rules, not into a single pattern with `|` (which the Chapter 9 subset does not support).

## The PEGParseResult

The result of `parse()` is a `PEGParseResult`, a small aggregate with success/failure flags, position information, and a message string. Its public interface, quoted from the header, is:

```cpp
struct PEGParseResult
{
  bool success{ true };
  std::size_t offset{ 0 };
  std::size_t line{ 1 };
  std::size_t column{ 1 };
  std::string message{};

  bool ok () const noexcept { return success; }
  bool failed () const noexcept { return !success; }
  std::size_t error_offset () const noexcept { return offset; }
  std::size_t error_line () const noexcept { return line; }
  std::size_t error_column () const noexcept { return column; }
  std::string_view error_message () const noexcept { return message; }
};
```

On success, `offset` holds the final position (equal to `text.size()` when the whole input was consumed), `success` is `true`, and `message` is empty. On failure, `success` is `false`, `offset`/`line`/`column` describe the position at which parsing stuck, and `message` is a human-readable diagnostic such as `"no PEG rule matched at offset 17"`. Callers typically branch on `ok()` or `failed()` and then format the diagnostic for the user.

The two failure modes are distinguishable by message. A failure with the message `"no PEG rule matched at offset N"` indicates that no visible rule could match at the current position — typically a syntax error or a missing rule. A failure with the message `"PEG rule matched empty input; refusing to loop"` indicates that a rule matched zero characters, which the engine refuses to accept because it would not advance the position. The latter almost always indicates a pattern bug, such as `[x]*` used without a following required atom.

## A Whitespace-Ignoring Tokenizer

The first complete example mirrors `examples/23-peg-definition.cpp`. It defines a grammar that skips whitespace, recognises a handful of keywords, identifiers, numbers, and operators, and parses a short input. Each rule prints its match; the whitespace rule is routed to the ignore channel.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int
main ()
{
  auto grammar = dsl::create_peg_definition ();

  auto &ws = grammar.add_rule<"[ \\t\\n]+"> (
      [](dsl::PEGMatch &m) { std::cout << "skip_ws:" << m.length () << "\n"; });
  ws.channel = dsl::PEGIgnoreChannel;

  auto &kw_if   = grammar.add_rule<"if">   ([](dsl::PEGMatch &m) { std::cout << "kw:"   << m.value << "\n"; });
  auto &kw_then = grammar.add_rule<"then"> ([](dsl::PEGMatch &m) { std::cout << "kw:"   << m.value << "\n"; });
  auto &ident   = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(
      [](dsl::PEGMatch &m) { std::cout << "id:"  << m.value << "\n"; });
  auto &number  = grammar.add_rule<"[0-9]+">(
      [](dsl::PEGMatch &m) { std::cout << "num:" << m.value << "\n"; });
  auto &plus    = grammar.add_rule<"+"> ([](dsl::PEGMatch &m) { std::cout << "op:" << m.value << "\n"; });
  auto &minus   = grammar.add_rule<"-"> ([](dsl::PEGMatch &m) { std::cout << "op:" << m.value << "\n"; });

  auto result = grammar.parse ("if x1 + 2 then 40");
  std::cout << (result.ok () ? "parse_ok" : "parse_failed") << "\n";
  std::cout << "offset=" << result.offset << "\n";
}
```

The ordering of the `add_rule` calls is significant. The keyword rules precede the identifier rule, so `if` and `then` are recognised as keywords rather than as identifiers. The whitespace rule is declared first but, because it is routed to `@IGNORE`, its position in the list only affects when it is tried relative to other rules; placing it first means whitespace is consumed before any token is attempted, which is the usual convention.

Running this grammar on `"if x1 + 2 then 40"` produces a sequence of `kw`, `id`, `op`, `num`, `kw`, and `num` lines, followed by `parse_ok`. The whitespace rule fires between tokens and prints `skip_ws` lines, but because it is on the ignore channel, a downstream consumer using the Chapter 28 matcher would not see those matches as tokens.

## A Keyword Grammar

Keywords are the simplest case of an ordering-sensitive grammar. Each keyword is a literal pattern, and the identifier rule is a broader class pattern. Declaring keywords first ensures they win over the identifier rule at positions where the input begins with a keyword.

```cpp
auto grammar = dsl::create_peg_definition ();
grammar.add_rule<"if">   ([](dsl::PEGMatch &) { /* emit IF    */ });
grammar.add_rule<"then"> ([](dsl::PEGMatch &) { /* emit THEN  */ });
grammar.add_rule<"else"> ([](dsl::PEGMatch &) { /* emit ELSE  */ });
grammar.add_rule<"end">  ([](dsl::PEGMatch &) { /* emit END   */ });
grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(
    [](dsl::PEGMatch &) { /* emit IDENT */ });
```

The literal patterns `"if"`, `"then"`, `"else"`, and `"end"` match exactly those spellings. The identifier pattern `[a-zA-Z_][a-zA-Z_0-9]*` would also match them, but because the keyword rules come first, the keyword rules win. Swapping the order would make the identifier rule consume every keyword, and the keyword rules would become unreachable dead code.

A subtle point: the literal pattern `"if"` matches the prefix `if` of the input `iffy`. Under PEG ordered choice, the keyword rule wins at the position of the `i`, consumes two characters, and leaves `fy` for the next iteration, where the identifier rule will match `fy`. This is rarely what is wanted for a real language, where keywords should be whole words. The Chapter 9 pattern subset has no word-boundary anchor, so the usual fix is a negative-lookahead atom: the pattern `"if!"` would match `if` only when not followed by an identifier character. Authors should test such patterns against inputs like `iffy` to confirm the intended behaviour.

## Identifiers and Numbers

Identifiers and numbers are the workhorse patterns of most tokenizers. The identifier pattern `[a-zA-Z_][a-zA-Z_0-9]*` matches a letter or underscore followed by zero or more letters, digits, or underscores. The number pattern `[0-9]+` matches one or more digits. Both are greedy: they consume as much matching input as possible.

```cpp
auto &ident  = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(
    [](dsl::PEGMatch &m) { std::cout << "id:"  << m.value << "\n"; });
auto &number = grammar.add_rule<"[0-9]+">(
    [](dsl::PEGMatch &m) { std::cout << "num:" << m.value << "\n"; });
```

Greedy matching means `[0-9]+` consumes the entire digit run of `12345` as a single match; it does not stop after `1` or `12`. This is the desired behaviour for a number token. For identifiers, the greedy run stops at the first character that is not a letter, digit, or underscore, which correctly terminates `x1` at the space.

Numbers with fractional parts, signs, or exponents require more elaborate patterns. Because the Chapter 9 subset lacks grouping, a floating-point pattern must be written as a flat sequence: `[0-9]+.[0-9]+` matches a decimal number with a fractional part, and `[0-9]+.![0-9]` would match an integer not followed by a decimal point and digit. Authors should compose such patterns carefully and test them, as the absence of grouping makes some common forms awkward to express.

## A Small Expression Grammar

Combining the pieces above yields a small expression tokenizer. The following grammar recognises identifiers, integer literals, the four arithmetic operators, and parentheses, ignoring whitespace between tokens. It does not build a tree; it simply prints each token. Tree construction is the job of the semantic action, which may use `dsl::leaf` and `dsl::node` from Chapter 11 to assemble an AST.

```cpp
auto grammar = dsl::create_peg_definition ();

auto &ws = grammar.add_rule<"[ \\t\\n]+">([](dsl::PEGMatch &) {});
ws.channel = dsl::PEGIgnoreChannel;

grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">([](dsl::PEGMatch &m) { std::cout << "id:"  << m.value << "\n"; });
grammar.add_rule<"[0-9]+">                ([](dsl::PEGMatch &m) { std::cout << "num:" << m.value << "\n"; });
grammar.add_rule<"+">                     ([](dsl::PEGMatch &m) { std::cout << "op:"  << m.value << "\n"; });
grammar.add_rule<"-">                     ([](dsl::PEGMatch &m) { std::cout << "op:"  << m.value << "\n"; });
grammar.add_rule<"*">                     ([](dsl::PEGMatch &m) { std::cout << "op:"  << m.value << "\n"; });
grammar.add_rule<"/">                     ([](dsl::PEGMatch &m) { std::cout << "op:"  << m.value << "\n"; });
grammar.add_rule<"(">([](dsl::PEGMatch &m) { std::cout << "lp:"   << m.value << "\n"; });
grammar.add_rule<")">([](dsl::PEGMatch &m) { std::cout << "rp:"   << m.value << "\n"; });

auto r = grammar.parse ("(a + 42) * (b - 7)");
std::cout << (r.ok () ? "ok" : "fail:" + std::string (r.error_message ())) << "\n";
```

Note that the operator rules are single-character literals. The Chapter 9 matcher treats `+`, `-`, `*`, `/`, `(`, and `)` as literal atoms, so each rule matches exactly that character. There is no ambiguity with quantifiers here, because the quantifier characters only have their special meaning when they follow an atom; as the sole content of a pattern, `+` is a literal plus sign.

A real expression parser would not stop at tokenisation. The semantic actions would push tokens onto a stack, and a higher-level driver would reduce the stack into an AST using precedence rules. That reduction is outside the scope of the PEG engine itself, which is intentionally a scanner; the recursive-structure parsing is the domain of the combinators in Chapters 23 through 25 or of hand-written logic over the token stream.

## Building a Token Vector

Printing tokens to `std::cout` is illustrative but not reusable. A more practical action captures a vector by reference and appends a structured token to it. The token type is arbitrary; the example below uses a simple struct with a kind tag and the matched text.

```cpp
struct Token { std::string kind; std::string text; std::size_t line; };

std::vector<Token> tokens;
auto emit = [&](std::string kind) {
    return [&tokens, kind = std::move (kind)](dsl::PEGMatch &m) {
        tokens.push_back (Token{ kind, std::string (m.value), m.line });
    };
};

auto grammar = dsl::create_peg_definition ();
grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> (emit ("ident"));
grammar.add_rule<"[0-9]+">                 (emit ("number"));
grammar.add_rule<"+">                      (emit ("plus"));
grammar.add_rule<"-">                      (emit ("minus"));

auto &ws = grammar.add_rule<"[ \\t]+">([](dsl::PEGMatch &) {});
ws.channel = dsl::PEGIgnoreChannel;

grammar.parse ("alpha + 3 - beta");
// tokens now holds: ident(alpha), plus(+), number(3), minus(-), ident(beta)
```

The `emit` helper returns a lambda bound to a specific kind string. Each rule's action is a distinct closure sharing the `tokens` vector by reference. Because the action is invoked synchronously during `parse()`, the vector is fully populated by the time `parse()` returns, and the order of tokens matches the order of matches in the input.

This pattern scales to grammars of moderate size. For larger grammars, the kind tag is often an enumeration rather than a string, and the token struct carries additional fields such as the begin offset and the channel. The `PEGMatch` record provides all of these, so the action can copy whichever fields are relevant.

## Building an AST with Actions

The semantic action is the natural place to construct AST nodes. The `dsl::ASTNode` value type from Chapter 10 and the `dsl::leaf` and `dsl::node` builders from Chapter 11 are available for this purpose. An identifier action might push a leaf node onto a stack; an operator action might pop two operands and push a binary node.

```cpp
#include "DSLtk.hpp"
#include <vector>
#include <string>

std::vector<dsl::ASTNode> stack;

auto push_leaf = [](std::string tag) {
    return [&stack, tag = std::move (tag)](dsl::PEGMatch &m) {
        stack.push_back (dsl::leaf (tag, std::string (m.value)));
    };
};

auto grammar = dsl::create_peg_definition ();
grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> (push_leaf ("ident"));
grammar.add_rule<"[0-9]+">                 (push_leaf ("number"));
```

The operator actions would then reduce the stack: pop two children, combine them with `dsl::node ("add", { lhs, rhs })`, and push the result. Because the PEG engine is a scanner and not a precedence-aware parser, the reduction logic must live in the action or in a driver that consumes the token stream. The combinators of Chapter 23 offer an alternative model in which precedence is expressed structurally rather than by post-hoc reduction.

The choice between the PEG scanner plus hand-written reduction, and the parser combinators of Chapter 23, is a design decision. The PEG engine is lighter and faster for tokenising; the combinators are more expressive for full grammar parsing. The two are designed to coexist, and the PEG engine's tokens can feed a combinator-based parser where appropriate.

## Derived Grammars

A derived grammar extends a parent grammar without mutating it. The factory `derive_peg_definition(parent)` returns a new `PEGDefinition` whose `parent` pointer references the parent. The derived grammar sees all parent rules first in the flattened rule list, followed by its own local rules, so parent behaviour is consulted before child overrides at every position.

```cpp
auto base = dsl::create_peg_definition ();
base.add_rule<"[a-zA-Z_]+">([](dsl::PEGMatch &) { /* ident */ });
base.add_rule<"[0-9]+">    ([](dsl::PEGMatch &) { /* number */ });

auto extended = dsl::derive_peg_definition (base);
extended.add_rule<"0x[0-9a-fA-F]+">([](dsl::PEGMatch &) { /* hex */ });

auto r = extended.parse ("0x1F");
```

Derived grammars are the PEG engine's answer to grammar inheritance. A base grammar might define the common lexical structure of a family of languages, and each derived grammar might add language-specific tokens. Because the parent is consulted first, a derived grammar can also effectively shadow a parent rule by adding a more specific rule earlier in its own local list — though true shadowing of a parent rule at the same position is not possible, since the parent rule wins by precedence.

The parent pointer is a raw `const PEGDefinition*`. The caller is responsible for ensuring the parent outlives the derived grammar. Deriving from a temporary parent is a common error: the derived grammar holds a dangling pointer, and `parse()` will exhibit undefined behaviour. The usual discipline is to keep both grammars in named variables with overlapping lifetimes.

## Rule Configuration After Insertion

The reference returned by `add_rule` is the primary handle for post-insertion configuration. Beyond setting the channel, callers may toggle `must_fail`, set `fire_on_neg`, or attach an `if_succeed` follow-up rule. These advanced fields are summarised here for completeness; their use is uncommon but supported.

`must_fail` inverts a rule's success condition. When `true`, the rule succeeds only when its pattern does not match at the current position, and it consumes one character on success. This is the engine's mechanism for negative lookahead at the rule level, distinct from the `!` quantifier which operates within a single pattern. A `must_fail` rule is useful for implementing "any character except those starting X" without enumerating the excluded set.

`fire_on_neg` controls whether a `must_fail` rule's action is invoked. By default, a negated rule fires no action, since there is no matched text to report. Setting `fire_on_neg = true` causes the action to run with a `PEGMatch` describing the single consumed character, which is occasionally useful for diagnostic logging.

`if_succeed` is a pointer to a second rule that is tried at the same position after the primary rule matches. If the follow-up rule also matches, its action is invoked with its own `PEGMatch`. The follow-up does not advance the position further; the position advances to the end of the primary match. This mechanism supports limited forms of lookahead-with-action within the definition model, though most grammars will not need it.

```cpp
auto &kw_if  = grammar.add_rule<"if"> ([](dsl::PEGMatch &) {});
auto &not_id = grammar.add_rule<"[a-zA-Z_0-9]">([](dsl::PEGMatch &) {});
not_id.must_fail = true;
kw_if.if_succeed = &not_id; // match "if" only when not followed by an ident char
```

These advanced features are entirely optional. A grammar that uses only patterns, actions, and channels will never touch `must_fail`, `fire_on_neg`, or `if_succeed`, and that is the expected case for the majority of users.

## Patterns and the Chapter 9 Subset

Every rule pattern is compiled by the same `CompiledPattern::compile` function used by the `dsl::pattern` engine of Chapter 9. The accepted syntax is therefore identical: literal characters, the `.` wildcard, character classes `[...]` with ranges (`a-z`) and `^` negation, and the quantifiers `*` (zero or more), `+` (one or more), `?` (zero or one), and `!` (zero-width negative lookahead). The anchors `^` and `$` are accepted and ignored.

What the subset does not provide is just as important. There is no alternation operator `|`: alternatives are expressed as separate rules, not within a single pattern. There is no parenthesised grouping: a quantifier applies only to the immediately preceding atom, so `(ab)+` is not expressible. There are no backreferences, no non-greedy quantifiers, and no named captures. Authors translating a regex to a PEG rule pattern should decompose alternatives into ordered rules and flatten any grouped quantifiers.

The `!` quantifier is the subset's negative lookahead. It does not consume input; it succeeds at the current position when its atom does not match. The pattern `"if!"` is therefore `if` followed by a negative lookahead on... nothing, because `!` must follow an atom. To express "if not followed by an identifier character", the correct pattern is `"if![a-zA-Z_0-9]"`, where the `!` applies to the class `[a-zA-Z_0-9]`. Authors should verify their understanding of `!` against small examples, as its zero-width semantics can be surprising.

Malformed patterns are rejected at `add_rule` time. The compiler sets a `valid` flag and an `error` string on the `CompiledPattern`, and `emplace_rule` throws `std::invalid_argument` with a message combining the error and the offending pattern. The only currently detected malformation is an unterminated character class (a missing `]`); other syntactic issues are not diagnosed at compile time and will simply produce a pattern that matches in unexpected ways.

## Anchors and Implicit Anchoring

PEG matching is implicitly anchored at the current position. There is no notion of searching for a pattern anywhere in the remaining input; a rule either matches starting exactly at `pos` or it fails. The `^` and `$` anchors of regex syntax are therefore redundant, and the compiler accepts and ignores them to ease porting of existing patterns.

This implicit anchoring is why the engine is a scanner rather than a searcher. Each iteration of the `parse` loop fixes a position, and the rules compete to match at that position. A rule whose pattern begins with `^` behaves identically to one without it; a rule ending with `$` matches only when the end of the pattern coincides with the end of input, which is already enforced by the greedy match reaching the end of the available text.

Authors sometimes attempt to use `^` to mean "match only at the start of the input". The engine does not honour that meaning; `^` is ignored. A rule that should match only at the start of input must be handled by driver logic outside the grammar, for example by parsing the first token separately and then parsing the remainder with the full grammar.

## Greedy Matching and Backtracking

The Chapter 9 matcher is greedy: each quantifier consumes as much matching input as possible. There is no backtracking across atoms within a pattern, and no backtracking across rules. Once a rule's `try_match` returns an end offset, that offset is the position the engine advances to; later rules are not tried at the original position, and the matching rule is not retried with a shorter match.

This makes the engine's behaviour easy to predict. A pattern `[a-z]+` followed by `[0-9]` will fail on input `abc` because the greedy `[a-z]+` consumes all three letters and leaves nothing for the required digit. There is no mechanism for `[a-z]+` to give back a character so that `[0-9]` can match. Authors must order atoms so that greedy quantifiers do not starve later atoms; the negative lookahead `!` is the usual tool for bounding a greedy run.

The absence of cross-rule backtracking is the practical consequence of ordered choice. Once a rule wins at a position, the engine commits to it. If the rule's match leads to a dead end later in the input, the engine does not revisit the position to try an alternative rule. This is faithful to PEG semantics and is one of the reasons PEG parsing is efficient: there is no speculative exploration.

## The if_succeed Hook

The `if_succeed` hook is the engine's constrained form of lookahead-with-action. When a rule with an `if_succeed` pointer matches, the engine immediately tries the pointed-to rule at the same position. If that rule also matches, its action runs with a `PEGMatch` covering the second rule's span. The position then advances to the end of the primary match, not the second match.

This is useful when a primary token should be recognised only when followed by a specific context, but the context should not be consumed as part of the token. For example, a rule for the keyword `if` might attach an `if_succeed` rule that looks for `(`, so that the action can record that the `if` begins a parenthesised condition. The `(` itself is then matched again by the normal `(` rule on the next iteration.

The `if_succeed` hook is a pointer to another rule in the same definition. The caller is responsible for ensuring the pointed-to rule outlives the referencing rule, which is automatic when both are stored in the same `PEGDefinition`. Pointing across definitions, especially from a derived grammar to a parent rule, is possible but fragile: the parent must outlive the derived grammar, and the pointed-to rule must remain in effect.

Most grammars will not need `if_succeed`. It is documented here so that readers encountering it in existing code understand its semantics, and so that authors facing a genuine lookahead-with-action requirement know it exists. For ordinary lookahead without an action, the `!` quantifier within a single pattern is simpler and sufficient.

## Pitfalls in Rule Ordering

Rule ordering is the most common source of surprising behaviour in PEG grammars. The engine's first-match-wins rule means that a broad rule placed early will shadow every narrower rule placed after it. The identifier-versus-keyword case is the canonical example, but the same issue arises whenever one pattern is a prefix language of another.

A less obvious case arises with operators. A rule for `<` followed by a rule for `<=` will never recognise `<=`, because the `<` rule matches first and consumes only the first character. The fix is to declare the longer operator first: `<=` before `<`, `==` before `=`, `->` before `-`. This longest-first ordering is a general principle for literal-prefix ambiguities.

Whitespace rules interact with ordering too. A whitespace rule placed after a token rule will not be tried at a position where the token rule matches, so whitespace between tokens is only consumed because the token rule fails on the whitespace character. Placing the whitespace rule first is the usual convention and ensures whitespace is skipped before any token is attempted. Routing it to `@IGNORE` then makes it invisible to consumers.

A subtle pitfall is the zero-width match. A rule whose pattern can match the empty string — for example `[a-z]*` at a position where the next character is a digit — will cause `parse()` to fail with the message `"PEG rule matched empty input; refusing to loop"`. The engine refuses to advance on a zero-width match, because doing so would loop forever. Patterns that can match empty should be rewritten to require at least one character (`[a-z]+`) or guarded by a following required atom.

## Pitfalls with Ignore-Channel Rules

Routing a rule to `@IGNORE` does not remove it from the ordered-choice loop; it only changes how its matches are reported. An ignore-channel rule must still match in order to advance the position. If the ignore rule is the only rule that can match at a given position and it fails, the parse fails, just as it would for a default-channel rule.

This means that an ignore-channel whitespace rule must cover every form of whitespace the input may contain. A rule for `[ \t]+` will not consume newlines, so an input containing a newline at a position where no other rule matches will cause the parse to fail. The broader pattern `[ \t\n\r]+` is safer and is the usual choice for cross-platform whitespace handling.

Ignore-channel rules still fire their actions. The `m.ignored` flag is set to `true`, but the action runs as normal. An action that does heavy work on every match will do that work for ignorable matches too; if that is not desired, the action should check `m.ignored` and return early. A no-op action is the simplest choice for whitespace and comments.

A final pitfall concerns channel assignment timing. The channel is read from the rule at match time, not at insertion time. Changing a rule's channel after some matches have already occurred affects subsequent matches but not past ones. In practice this is rarely an issue because channels are configured before parsing begins, but it is worth noting that the channel is a mutable property of the rule.

## Derivation and Shadowing

Derived grammars inherit parent rules, but they cannot truly shadow a parent rule at the same position. Because parent rules come first in the flattened list, a parent rule that matches at a position will always win over a child rule that could also match there. A child grammar can only add new rules for cases the parent does not cover, or override parent behaviour by mutation of the parent rule (which requires non-const access to the parent).

This is a deliberate design choice. It makes derivation additive and non-destructive: a parent grammar's behaviour is preserved exactly when viewed through a derived grammar. Callers who need true shadowing should construct a fresh grammar rather than deriving, or should restructure the parent so that the shadowed rule is less specific and can be overridden by a more specific child rule placed earlier.

The `rules()` accessor flattens parent and child rules into a single vector, which `parse()` iterates over. There is no per-iteration lookup of parent versus child; the flattened list is the complete rule set as far as the engine is concerned. This keeps the parse loop simple and avoids virtual dispatch or rule-lookup overhead.

When a derived grammar is itself used as a parent for further derivation, the flattening recurses through the chain. A grandchild grammar sees its parent's rules (which include the grandparent's rules) first, then its own. The depth of the chain is not bounded by the engine, but very deep chains will produce large flattened lists on every `rules()` call, which is something to keep in mind for performance-sensitive uses.

## Lifetime and Reference Stability

The `PEGRule` references returned by `add_rule` are stable as long as the definition's internal vector does not reallocate. Because rules are only appended, the vector may reallocate as it grows, but the engine uses a `std::vector<PEGRule>` whose references are invalidated by reallocation. In practice, callers should obtain references after all rules have been added, or should reserve capacity in advance if the engine exposed such a hook.

The engine does not currently expose a `reserve` method. Callers who need absolute reference stability across many insertions may wish to add rules in batches, or to store rules by pointer in their own structures. For the typical grammar of a few dozen rules, reallocation is unlikely to be a problem, but it is a theoretical concern worth noting.

The `PEGDefinition` itself is movable. Moving a definition transfers ownership of its rules to the target, and references obtained before the move are invalidated. The `parent` pointer of a derived grammar is a raw pointer to the parent object; if the parent is moved, the pointer still refers to the original location, which is now empty. Callers should avoid moving a grammar that is referenced as a parent by a derived grammar.

The `if_succeed` pointer is a raw `PEGRule*`. It must point to a rule that outlives the referencing rule. Within a single definition, this is automatic. Across definitions, the caller must ensure the pointed-to rule's definition outlives the referencing rule's definition. The engine does not track these dependencies; misuse leads to dangling pointers and undefined behaviour.

## Integration with the Matcher

The `PEGDefinition` is the input to the `PEGMatcher` described in Chapter 28. The matcher consumes a grammar and an input text and applies a stream of arms — each binding a rule (or a wildcard, or a channel filter) to a callback — in ordered-choice fashion. The matcher is the natural consumer of a grammar when the caller wants per-rule control over which actions fire, as opposed to the actions installed on the rules themselves.

The relationship between the two layers is simple: the definition owns the rules and their patterns; the matcher owns the arms and their callbacks. A rule's installed semantic action fires during `PEGDefinition::parse()`; a matcher arm's callback fires during `PEGMatcher::operator<<`. The two are independent, and a grammar may be used with either or both.

Chapter 28 covers the matcher, the `PEGMatch` record from the matcher's perspective, channel filtering, and the wildcard arm. This chapter has covered the definition and its rules; the matcher is the next step for readers who need finer-grained control over match dispatch than the installed actions provide.

## A Complete Tokenizer Example

The following example assembles the chapter's material into a complete, self-contained tokenizer. It defines a grammar for a tiny expression language with keywords, identifiers, numbers, operators, and parentheses, ignores whitespace, collects tokens into a vector, and prints them.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>
#include <vector>

struct Token { std::string kind; std::string text; std::size_t line; };

int
main ()
{
  std::vector<Token> tokens;
  auto emit = [&](std::string kind) {
      return [&tokens, kind = std::move (kind)](dsl::PEGMatch &m) {
          tokens.push_back (Token{ kind, std::string (m.value), m.line });
      };
  };

  auto grammar = dsl::create_peg_definition ();

  auto &ws = grammar.add_rule<"[ \\t\\n\\r]+">([](dsl::PEGMatch &) {});
  ws.channel = dsl::PEGIgnoreChannel;

  grammar.add_rule<"if">   (emit ("kw_if"));
  grammar.add_rule<"then"> (emit ("kw_then"));
  grammar.add_rule<"else"> (emit ("kw_else"));
  grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> (emit ("ident"));
  grammar.add_rule<"[0-9]+">                 (emit ("number"));
  grammar.add_rule<"+"> (emit ("plus"));
  grammar.add_rule<"-"> (emit ("minus"));
  grammar.add_rule<"*"> (emit ("star"));
  grammar.add_rule<"/"> (emit ("slash"));
  grammar.add_rule<"("> (emit ("lparen"));
  grammar.add_rule<")"> (emit ("rparen"));

  auto r = grammar.parse ("if alpha + 3 then beta else 7");
  if (r.failed ())
    {
      std::cout << "parse failed at line " << r.error_line ()
                << " col " << r.error_column () << ": "
                << r.error_message () << "\n";
      return 1;
    }

  for (const auto &t : tokens)
    std::cout << t.line << ": " << t.kind << " '" << t.text << "'\n";
}
```

The grammar is ordered from most specific to least specific: keywords before identifiers, single-character operators before any broader class. The whitespace rule is first and routed to `@IGNORE`, so it consumes inter-token spacing without producing a token. The `emit` helper binds a kind string into each action, so each rule produces a token with the appropriate tag.

Running this program on `"if alpha + 3 then beta else 7"` produces a token list: `kw_if`, `ident(alpha)`, `plus`, `number(3)`, `kw_then`, `ident(beta)`, `kw_else`, `number(7)`. The line numbers are all `1` because the input is a single line. The whitespace between tokens is consumed but does not appear in the vector, because the whitespace rule is on the ignore channel and its action is a no-op.

## Summary

The PEG definition grammar is a lightweight, additive engine for tokenising and small-scanner tasks. A `PEGDefinition`, created with `create_peg_definition()`, holds an ordered set of `PEGRule` values, each carrying a compile-time pattern (a `FixedString` NTTP in the Chapter 9 regex subset), a semantic action of signature `void(PEGMatch&)`, and a `PEGChannel`. Rules are added with `add_rule<Pattern>(action)`, which returns a reference for post-insertion configuration. The `parse()` entry point runs ordered choice over the flattened rule list: the first matching rule at each position wins, its action fires, and the position advances. Channels categorise or suppress matches: `PEGDefaultChannel` for ordinary tokens, `PEGIgnoreChannel` for whitespace and comments, and custom channels created with `new_peg_channel` for caller-defined categories. Derived grammars, created with `derive_peg_definition(parent)`, extend a parent without mutating it. The `PEGParseResult` reports success or failure with offset, line, column, and a human-readable message. Rule ordering is paramount: specific rules must precede general ones, longer literals must precede shorter prefixes, and zero-width matches are rejected to prevent infinite loops. The engine integrates with the `PEGMatcher` of Chapter 28, which provides stream-style dispatch over the same rule set, and with the AST facilities of Chapters 10 and 11 for callers who construct trees from their semantic actions.
