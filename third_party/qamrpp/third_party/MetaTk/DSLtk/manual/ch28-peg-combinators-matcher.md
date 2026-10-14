# Chapter 28: PEG Combinators, `PEGMatch`, and the `PEGMatcher`

Chapter 27 introduced the `PEGDefinition` grammar: an ordered set of rules, each compiled from a `dsl::pattern` regex subset, each carrying an optional semantic action and a channel. This chapter completes the PEG story. It describes the `PEGMatch` record that the engine hands to every action, the `PEGMatcher` that walks an input string arm by arm, and the `peg_*` combinator family that lifts the same grammar into the parser-combinator world of Chapters 23 through 25.

Where Chapter 27 was about *declaring* a grammar, this chapter is about *running* one. Two execution paths are presented. The first is `PEGDefinition::parse`, a self-contained driver that applies ordered choice across all visible rules until input is exhausted. The second is `PEGMatcher`, a fluent, stream-style matcher whose `operator<<` consumes one match arm at a time and whose wildcard arm recovers from otherwise unmatched input. Both paths share `PEGMatch` as their reporting record and both honour the channel discipline that routes ignore matches silently.

The chapter closes with the `peg_seq`, `peg_choice`, `peg_opt`, `peg_many`, `peg_many1`, `peg_and`, and `peg_not` combinators. These are thin, additive wrappers over the `dsl::Parser` machinery of Chapter 23, re-expressed with strict PEG semantics: ordered choice, full backtracking on partial failure, and zero-width lookahead. They allow a grammar author to mix the rule-based PEG engine with the combinator-based parser engine in a single program.

## The `PEGMatch` Record

Every semantic action in the PEG engine has the signature `void(PEGMatch&)`. The `PEGMatch` is the engine's report of one successful match: the text consumed, the offsets that bound it, the rule that produced it, the channel it was routed to, and the line and column at which it begins. It is the single channel through which a grammar author observes what the engine did.

The structure is deliberately small and copyable. Its most important member, `value`, is a `std::string_view` into the source text being parsed. The view is only valid for the lifetime of that source text and for the duration of the parse that produced it; an action that stores `value` for later inspection must copy the characters into owning storage.

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

  /// @return length of the matched text (end - begin).
  std::size_t
  length () const noexcept
  {
    return end - begin;
  }
};
```

The `begin` and `end` members give the half-open offset range in the source. `length()` is exactly `end - begin`; it is the value that example 23 prints in its `skip_ws:` line. The `rule` pointer names the `PEGRule` whose pattern produced the match; for wildcard arms fired by the `PEGMatcher`, `rule` is `nullptr`. The `channel` member is the resolved channel of the rule at fire time, and `ignored` is a convenience boolean equivalent to `channel.is_ignore()`.

The `user_data` slot is an opaque extension point. The engine never reads or writes it; it exists so that an embedding application can attach auxiliary state to a match without subclassing `PEGMatch` or threading separate context through every action. A common use is to store a pointer to a token object that the action has just allocated.

The `line` and `column` members are computed by `peg_detail::line_col_at`, which scans the source text from offset zero up to `begin` counting newlines. They are 1-based. Their cost is linear in `begin`, which is acceptable for the line-oriented grammars the PEG engine targets; an embedding that finds this cost significant should maintain its own line cache.

### Reading `value` and `length`

Example 23 in the distribution is the canonical demonstration of an action reading a `PEGMatch`. The whitespace rule is registered with the `@IGNORE` channel and its action prints the length consumed; the keyword rules print their matched `value`.

```cpp
auto grammar = dsl::create_peg_definition ();
auto &ws = grammar.add_rule<"[ \\t\\n]+"> ([] (dsl::PEGMatch &match)
                                           { std::cout << "skip_ws:" << match.length () << "\n"; });
ws.channel = dsl::PEGIgnoreChannel;

auto &kw_if = grammar.add_rule<"if"> ([] (dsl::PEGMatch &match)
                                      { std::cout << "kw:" << match.value << "\n"; });
```

Given the input `"if x1 + 2 then 40"`, the engine first matches `if` and prints `kw:if`. It then matches the space, prints `skip_ws:1`, and continues. The whitespace action fires even though the channel is `@IGNORE`; the channel controls token *emission*, not action invocation. This distinction is fundamental and is revisited below.

The `value` view is exactly `text.substr(begin, end - begin)`. An action that needs the matched text as a `std::string` writes `std::string(match.value)`. An action that needs the character at a specific offset uses `match.value[i]` for `i` in `[0, match.length())`. No bounds checking is performed beyond the inherent bounds of `std::string_view`.

### The `rule` Back-Reference

The `rule` member lets an action inspect the rule that fired it. This is useful when several rules share a single action function and the action must distinguish between them, or when the action wants to read the rule's `pattern_text` for diagnostics. The pointer is `const_cast` away from `const` only because the action signature takes a non-const reference; an action should not mutate the rule's pattern or compiled form from within the match.

The `if_succeed` hook described in Chapter 27 fires a second rule's action when the primary rule matches. In that case the primary match record carries the primary rule's pointer; the secondary match record, constructed separately inside `PEGDefinition::parse`, carries the `if_succeed` rule's pointer. Both actions fire, in that order, at the same source position.

## The `PEGMatcher` Engine

The `PEGMatcher` is a fluent, stream-style matcher over a `PEGDefinition` and a source string. Where `PEGDefinition::parse` runs the whole input to completion in one call, `PEGMatcher` advances one match at a time, giving the caller control over which rules to try and in what order, at every position. It is the engine of choice when the grammar author wants per-position customisation or wildcard recovery.

The matcher is created by `dsl::peg_new_matcher`, which binds a reference to the definition and a copy of the source view. The caller then chains arms with `operator<<` until the input is spent, and finally calls `close()`. The `close()` call is a no-op preserved for API symmetry with stream-style interfaces; it releases nothing because the matcher holds nothing that needs releasing.

```cpp
struct PEGMatcher
{
  const PEGDefinition *def{ nullptr };
  std::string_view text{};
  std::size_t pos{ 0 };

  bool
  is_spent () const noexcept
  {
    return pos >= text.size ();
  }
  std::size_t
  position () const noexcept
  {
    return pos;
  }
  /// No-op for API symmetry; releases nothing (nothing is held).
  void
  close () noexcept
  {
  }
  // ...
};
```

The public state is the cursor `pos`. The matcher advances `pos` only when an arm fires. Until then the cursor is stationary and the accumulated arms are retained for the next `step()`. This retention is what allows arms to be chained across multiple `<<` calls within a single source position: the matcher waits until one of them matches before consuming input.

The matcher does not own the `PEGDefinition` it references. The definition must outlive the matcher. Because `PEGDefinition::add_rule` returns references that are stable across appends, a common pattern is to build the grammar once, store it in a long-lived object, and create a fresh `PEGMatcher` for each input string.

### The Match Arm

An arm is a lightweight record consumed by `operator<<`. It is constructed by calling a `PEGRule` with a callback, which yields a `peg_detail::PEGArm` bound to that rule and that action. The matcher also provides a `wildcard(Fn)` helper that produces an arm with `is_wildcard` set and no rule attached.

```cpp
auto m = dsl::peg_new_matcher (grammar, source);
while (!m.is_spent ())
  {
    m << keyword
        ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
      << number
             ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
      << ident ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
      << m.wildcard ([&] (dsl::PEGMatch &match)
                     { std::cout << "[unknown:'" << match.value << "']"; });
  }
m.close ();
```

Each `rule(callback)` expression produces a fresh arm. Arms accumulate in the matcher until one fires, at which point the accumulator is cleared and the cursor advances. If no rule arm matches at the current position, the first wildcard arm in the accumulator fires and consumes exactly one character. This is the recovery mechanism: a wildcard arm at the end of the chain guarantees forward progress.

The `PEGArm` structure carries a `rule` pointer, an `action`, the `is_wildcard` flag, and an `is_channel` flag with an attached `PEGChannel`. Channel arms are produced by the matcher's channel-filter API and match any rule whose channel name agrees; they are described below.

### Ordered Choice Across Arms

The `step()` method is the heart of the matcher. It first skips ignored-channel matches, then walks the accumulated arms in order. The first arm whose rule matches at the current position fires its action, advances the cursor, and clears the accumulator. This is ordered choice at the arm level: the order in which arms are streamed into the matcher is the order of precedence.

```cpp
void
step ()
{
  if (arms_.empty () || is_spent ())
    return;
  skip_ignored ();
  if (is_spent ())
    {
      arms_.clear ();
      return;
    }
  // Non-wildcard arms in order: first match wins.
  for (std::size_t i = 0; i < arms_.size (); ++i)
    {
      const auto &arm = arms_[i];
      if (arm.is_wildcard)
        continue;
      // ... channel and rule branches ...
      if (auto end = arm.rule->try_match (text, pos))
        {
          fire (arm.action, arm.rule, pos, *end);
          arms_.clear ();
          return;
        }
    }
  // No rule arm matched: fire the first wildcard arm, if any.
  // ...
}
```

The arms list is walked twice. The first walk skips wildcards and tests each rule arm. The second walk fires the first wildcard arm if no rule arm matched. This ordering means that a wildcard placed in the middle of the chain does not preempt later rule arms; only when *all* rule arms have failed does the wildcard fire. A chain with no wildcard and no matching rule arm leaves the accumulator intact and the cursor stationary, waiting for the next `<<` to supply a further arm.

This last behaviour is subtle. It permits a caller to assemble an arm set incrementally across several statements, only committing to a step once a matching arm has been streamed. It also means that a chain with no wildcard and no match will loop forever if the caller keeps re-streaming the same arms; the caller must either include a wildcard or break out of the loop on a stationary cursor.

### Ignored-Channel Skipping

Before any arm is tested, `step()` calls `skip_ignored()`. This method walks every rule in the definition whose channel is `@IGNORE` and applies it at the current position. If any such rule matches and consumes at least one character, the cursor advances and the walk repeats. The loop terminates when no ignore rule makes progress.

```cpp
void
skip_ignored ()
{
  bool progressed = true;
  while (progressed && !is_spent ())
    {
      progressed = false;
      for (const PEGRule *r : def->rules ())
        {
          if (!r->channel.is_ignore ())
            continue;
          if (auto end = r->try_match (text, pos))
            {
              if (*end == pos)
                continue;
              pos = *end;
              progressed = true;
              break;
            }
        }
    }
}
```

The zero-width check (`*end == pos`) is critical. An ignore rule that matches the empty string would otherwise loop forever, advancing the cursor by zero on each iteration. The matcher explicitly skips such matches and continues to the next rule. The same zero-width protection appears in `PEGDefinition::parse`, where it aborts the parse with an explicit error rather than silently looping.

Note that `skip_ignored` does *not* fire ignore-rule actions. It advances the cursor silently. This is a deliberate choice for the matcher path: the matcher's arms own the actions for the positions they match, and ignore matches are treated as pure whitespace stripping. The `parse` driver, by contrast, does fire ignore-rule actions, which is why example 23's `skip_ws:` line appears in its output.

### The `fire` Helper

When an arm matches, `step()` delegates to `fire()` to construct the `PEGMatch` record and invoke the action. The helper fills every field of the record, advances the cursor to the match end, and then calls either the arm's own action (if one was supplied with the arm) or the rule's stored semantic action.

```cpp
void
fire (const PEGSemanticAction &action, const PEGRule *rule,
      std::size_t begin, std::size_t end)
{
  PEGMatch m;
  m.value = text.substr (begin, end - begin);
  m.begin = begin;
  m.end = end;
  m.rule = const_cast<PEGRule *> (rule);
  m.channel = rule->channel;
  m.ignored = rule->channel.is_ignore ();
  peg_detail::line_col_at (text, begin, m.line, m.column);
  pos = end;
  if (action)
    action (m);
  else if (rule->semantic_action)
    rule->semantic_action (m);
}
```

The precedence between arm action and rule action is significant. An arm constructed as `rule(callback)` carries its own action, which overrides the rule's stored semantic action for that arm. An arm constructed without a callback would fall back to the rule's stored action. In practice every arm supplies a callback, but the fallback exists so that a rule authored with a strong action can be reused in a matcher without re-specifying the action.

The cursor is advanced before the action runs. An action that inspects `m.end` therefore sees the same value the matcher will use for the next position. An action that wishes to "reject" a match after the fact cannot do so by mutating the match record; the engine offers no backtracking channel out of an action. If an action needs to suppress a match, it must do so by cooperating with the grammar structure, for example by routing the rule to a channel the caller filters out.

### The Wildcard Arm

When no rule arm matches, `step()` walks the accumulator a second time looking for the first wildcard arm. The wildcard constructs a `PEGMatch` covering exactly one character at the current position, fires the wildcard's action, advances the cursor by one, and clears the accumulator.

```cpp
PEGMatch m;
m.value = text.substr (pos, 1);
m.begin = pos;
m.end = pos + 1;
m.rule = nullptr;
peg_detail::line_col_at (text, pos, m.line, m.column);
if (arms_[i].action)
  arms_[i].action (m);
++pos; // wildcard consumes one character
arms_.clear ();
return;
```

The wildcard's `rule` pointer is `nullptr`, which lets an action distinguish a wildcard fire from a rule fire. The wildcard consumes exactly one character regardless of what that character is; this is the matcher's guarantee of forward progress. A chain that ends with a wildcard never stalls and never needs an external break condition.

The wildcard is the matcher's error-recovery mechanism. In a tokenizer, a wildcard arm typically emits a "unknown character" diagnostic and continues, so that a single unexpected character does not abort tokenization of the rest of the input. The `parse` driver has no equivalent; it stops at the first unmatched character.

## Channel Filtering

The `PEGMatcher` provides three channel-filter mutators: `only`, `include`, and `exclude`. Each returns a reference to the matcher so that it can be chained before the matching loop. The filter restricts which rule arms are considered by `step()`; it does not affect `skip_ignored`, which always considers all ignore rules regardless of filter.

```cpp
PEGMatcher &
only (std::string_view channel)
{
  mode_ = FilterMode::Only;
  channels_.clear ();
  channels_.push_back (std::string (channel));
  return *this;
}
```

The `only` mode restricts matching to rules on a single named channel. The `include` mode adds a channel to the accepted set, switching from `All` to `Only` if necessary. The `exclude` mode rejects rules on the named channel, accepting all others. The `channel_allowed` predicate encodes these semantics: in `Only` mode a channel is allowed iff it appears in the list; in `Exclude` mode iff it does not.

Channel filters are evaluated against the rule's resolved channel name. A rule on `@DEFAULT` is allowed under `only("@DEFAULT")` and excluded under `exclude("@DEFAULT")`. Ignore rules are always skipped by `skip_ignored` before the filter is consulted, so filtering `@IGNORE` has no effect on the matcher's behaviour; the ignore rules still run.

Example 24 in the distribution uses `only("@DEFAULT")` to restrict a matcher to ordinary tokens. This is a common pattern when a grammar defines comment or whitespace rules on `@IGNORE` and the caller wants to enumerate only the semantic tokens. The filter is set once before the loop and applies to every step.

### Channel Arms

In addition to filtering, the matcher supports channel *arms*. A channel arm is constructed by the matcher's internal channel-arm machinery and matches any rule on a named channel, in declaration order. When `step()` encounters a channel arm, it walks the definition's rules looking for the first one whose channel name matches and whose pattern matches at the current position.

```cpp
if (arm.is_channel)
  {
    for (const PEGRule *r : def->rules ())
      {
        if (r->channel.name != arm.channel.name)
          continue;
        if (auto end = r->try_match (text, pos))
          {
            fire (arm.action, r, pos, *end);
            arms_.clear ();
            return;
          }
      }
    continue;
  }
```

A channel arm is a concise way to say "match whatever token is next on this channel" without enumerating the individual rules. It is the channel analogue of the wildcard: where the wildcard matches any single character, a channel arm matches any single rule on its channel. Channel arms compose with rule arms in the same accumulator and obey the same first-match-wins ordering.

## The `parse` Driver

The `PEGDefinition::parse` method is the all-in-one driver. It takes a source string, walks it from offset zero, and at each position tries every visible rule in order until one matches. On success it advances the cursor and continues; on failure it records the offset, line, and column and returns. It succeeds when the entire input is consumed.

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
          // ... fire action, advance, break ...
        }
      // ...
    }
  result.offset = pos;
  return result;
}
```

The driver has no wildcard recovery. If no rule matches at the current position, the parse fails immediately. The error record carries the offset, the 1-based line and column, and a message of the form `"no PEG rule matched at offset N"`. The caller inspects `result.ok()`, `result.failed()`, `result.error_offset()`, `result.error_line()`, `result.error_column()`, and `result.error_message()` to diagnose the failure.

The driver fires ignore-rule actions in line with non-ignore actions. The channel does not suppress action invocation; it only marks the match as ignored. An action that wishes to skip work on ignored matches checks `match.ignored` or `match.channel.is_ignore()` and returns early.

### Zero-Width Protection

The parse driver explicitly rejects zero-width positive matches. If a rule matches and consumes no input, the driver sets `success` to false, records the position, and returns with the message `"PEG rule matched empty input; refusing to loop"`. This is a hard error, not a warning: the parse cannot continue because the cursor would not advance.

```cpp
if (!negated && *end == pos)
  {
    result.success = false;
    result.offset = pos;
    peg_detail::line_col_at (text, pos, result.line, result.column);
    result.message = "PEG rule matched empty input; refusing to loop";
    return result;
  }
```

This protection is essential for patterns that can match the empty string, such as `[a-z]*` applied at a position where no letter follows. A pattern of that form is almost always a grammar bug. The library refuses to guess whether the author intended a `+` quantifier or an explicit lower bound; it reports the situation and lets the author fix the grammar.

Negated rules (`must_fail`) are exempt from this check because their success consumes one character by construction, as described in Chapter 27. The `if_succeed` hook is also exempt because it runs at the same position as the primary rule and does not independently advance the cursor.

### The `if_succeed` Hook

When a rule with an `if_succeed` hook matches, the driver attempts the hooked rule at the same position. If the hooked rule also matches, its action fires with a match record covering its own span. The cursor then advances to the *primary* rule's end, not the hooked rule's end; the hook is a side channel for additional matching, not a sequence operator.

```cpp
if (r->if_succeed)
  {
    if (auto e2 = r->if_succeed->try_match (text, pos))
      {
        PEGMatch m;
        m.value = text.substr (pos, *e2 - pos);
        m.begin = pos;
        m.end = *e2;
        m.rule = r->if_succeed;
        m.channel = r->if_succeed->channel;
        m.ignored = r->if_succeed->channel.is_ignore ();
        peg_detail::line_col_at (text, pos, m.line, m.column);
        if (r->if_succeed->semantic_action)
          r->if_succeed->semantic_action (m);
      }
  }
pos = *end;
```

A typical use of `if_succeed` is to attach a lookahead-style action: when a keyword matches, the hook can inspect what follows without consuming it. The hook's match record covers the lookahead span, but the cursor advance uses the primary match's `end`. If the hook fails, the primary match still stands and the cursor still advances; the hook is best-effort.

The action firing order is therefore: primary action first, then `if_succeed` action. Both fire at the same source position. The wildcard arm in the `PEGMatcher` does not support `if_succeed`; the hook is a property of `PEGRule` and the matcher's wildcard has no rule.

## Derived Grammars and Rule Ordering

Chapter 27 introduced `dsl::derive_peg_definition` for grammar inheritance. The derived definition sees parent rules first, then its own local rules, in that order. The `rules()` method assembles this list by recursing through the `parent` pointer.

```cpp
std::vector<const PEGRule *>
rules () const
{
  std::vector<const PEGRule *> out;
  if (parent)
    out = parent->rules ();
  for (const auto &r : local_rules_)
    out.push_back (&r);
  return out;
}
```

Because ordered choice takes the first matching rule, parent rules take precedence over child rules at any decision point. A child grammar that wishes to override a parent rule must add a more specific rule that matches before the parent rule is reached. Because the parent's rules come first in the list, a child cannot override by simple redeclaration; it must shadow by writing a rule that matches a strict superset of the parent's pattern and that appears earlier in the list.

In practice, derived grammars are usually additive: the parent defines the common tokens and the child adds new ones. Example 23 derives a grammar that adds a `0x[0-9a-fA-F]+` hex literal rule to the base keyword-and-identifier grammar. The derived grammar parses input that the base grammar cannot, while still recognising everything the base grammar recognises.

The `PEGMatcher` respects derivation automatically because it queries `def->rules()` for both `skip_ignored` and channel-arm matching. A matcher over a derived grammar sees the same merged rule list that `parse` sees. There is no separate API for derived matchers; the same `peg_new_matcher` works for both root and derived definitions.

## The PEG Combinator Family

The PEG engine described so far is rule-based: patterns are compiled at runtime from string literals. The library also provides a combinator family that expresses the same PEG semantics through the `dsl::Parser` machinery of Chapter 23. These combinators are thin, additive wrappers; they do not replace the parser combinators but re-express them with strict PEG semantics for use in grammars that mix the two styles.

The combinators live in the `dsl` namespace and all produce `Parser<PEGMatch>` (or, for the lookahead operators, `Parser<bool>`). They take `Parser<T>` arguments and return `Parser<PEGMatch>`, so they compose with each other and with the primitive parsers of Chapter 24. The `peg_rule` adapter converts a `PEGRule` into a `Parser<PEGMatch>`, bridging the rule-based and combinator-based worlds.

### Sequence: `peg_seq`

The sequence combinator parses its arguments in order and fails if any of them fails. On success it returns a `PEGMatch` whose span covers the entire sequence. The combinator is variadic: `peg_seq(a, b, c)` parses `a`, then `b`, then `c`, and returns the concatenated span.

```cpp
template <typename A, typename B, typename... Rest>
auto
peg_seq (const Parser<A> &a, const Parser<B> &b, const Rest &...rest)
{
  auto ab = parser ([a, b] (ParsecInput &in) -> ExpectedResult<PEGMatch> {
    std::size_t begin = in.pos;
    auto ra = a (in);
    if (!ra)
      return ExpectedResult<PEGMatch>::failure (ra.error.pos, ra.error.kind,
                                                ra.error.expected);
    auto rb = b (in);
    if (!rb)
      {
        bool consumed = in.pos != begin;
        in.pos = begin;
        auto kind = rb.error.kind;
        if (consumed)
          kind = ParseFailureKind::Committed;
        return ExpectedResult<PEGMatch>::failure (rb.error.pos, kind,
                                                  rb.error.expected);
      }
    PEGMatch m;
    m.begin = begin;
    m.end = in.pos;
    m.value = in.get_span (begin);
    return m;
  });
  if constexpr (sizeof...(rest) == 0)
    return ab;
  else
    return peg_seq (ab, rest...);
}
```

The key behaviour is the backtracking on failure. If `b` fails after `a` succeeded, the cursor is restored to `begin` before the failure is reported. The failure kind is upgraded to `Committed` if `a` consumed input, signalling to an outer `try_parse` that the failure is not recoverable at this position. This is the PEG sequence semantics: a sequence commits once it has consumed input, and a later failure cannot be backtracked over by an enclosing choice.

The recursive expansion folds the variadic list left-to-right. `peg_seq(a, b, c)` becomes `peg_seq(peg_seq(a, b), c)`. The leftmost argument is parsed first and the rightmost last, matching the reading order of a grammar rule `a b c`.

### Ordered Choice: `peg_choice`

The choice combinator implements PEG ordered choice. Each alternative is wrapped in `try_parse`, which converts a committed failure back into a soft failure that the choice can backtrack over. The first alternative that succeeds wins; later alternatives are not tried at that decision point.

```cpp
template <typename A, typename B, typename... Rest>
auto
peg_choice (const Parser<A> &a, const Parser<B> &b, const Rest &...rest)
{
  auto ab = try_parse (a) | try_parse (b);
  if constexpr (sizeof...(rest) == 0)
    return ab;
  else
    return peg_choice (ab, rest...);
}
```

The `try_parse` wrapper is the bridge between the parser-combinator error model of Chapter 25 and the PEG backtracking model. Without it, a committed failure from one alternative would propagate through the `|` operator and abort the choice. With it, every alternative is given a fresh chance at the decision point, and only a truly soft failure (one that consumed no input) is recoverable.

The choice combinator is the analogue of the `PEGMatcher`'s ordered arm list and of the `PEGDefinition::parse` driver's rule loop. All three implement the same first-match-wins semantics. The combinator form is useful when the alternatives are themselves combinator expressions rather than compiled pattern strings.

### Optional and Repetition

Three repetition combinators cover the standard PEG quantifiers. `peg_opt` implements `a?`: it never fails and never commits, matching zero or one `a`. `peg_many` implements `a*`: greedy zero-or-more, with full backtracking on a partial failure. `peg_many1` implements `a+`: greedy one-or-more, failing if the first iteration fails.

```cpp
template <typename T>
auto
peg_opt (const Parser<T> &p)
{
  return optional (try_parse (p));
}

template <typename T>
auto
peg_many (const Parser<T> &p)
{
  return *try_parse (p);
}

template <typename T>
auto
peg_many1 (const Parser<T> &p)
{
  return p & *try_parse (p);
}
```

The `try_parse` inside `peg_opt` and `peg_many` ensures that a partial failure of `p` does not commit the surrounding context. A `peg_many(p)` that matches three iterations and then fails on the fourth backtracks the cursor to the end of the third iteration and reports success with three matches. This is the PEG repetition semantics: greedily match as many as possible, then stop.

The `peg_many1` form is `p & *try_parse(p)`: one unguarded `p` followed by zero-or-more guarded `p`. The first `p` must succeed for the whole expression to succeed. If the first `p` fails, the failure propagates with whatever kind `p` produced; if a later `p` fails, the `try_parse` wrapper makes the failure recoverable and the repetition terminates.

### Lookahead: `peg_and` and `peg_not`

The two lookahead combinators implement zero-width assertions. `peg_and` succeeds if its argument matches and consumes nothing. `peg_not` succeeds if its argument fails and consumes nothing. Both restore the cursor to its saved position regardless of outcome.

```cpp
template <typename T>
auto
peg_and (const Parser<T> &p)
{
  return parser ([p] (ParsecInput &in) -> ExpectedResult<bool> {
    auto save = in.pos;
    auto r = p (in);
    in.pos = save;
    if (!r)
      return ExpectedResult<bool>::failure (r.error.pos, ParseFailureKind::Soft,
                                            r.error.expected);
    return true;
  });
}

template <typename T>
auto
peg_not (const Parser<T> &p)
{
  return parser ([p] (ParsecInput &in) -> ExpectedResult<bool> {
    auto save = in.pos;
    auto r = p (in);
    in.pos = save;
    if (r)
      return fail_expected<bool> (in, "negative lookahead");
    return true;
  });
}
```

Lookahead is the PEG mechanism for context-sensitive matching. `peg_and(digit)` asserts that the next character is a digit without consuming it; `peg_not(ch('-'))` asserts that the next character is not a minus. Both report their result as `Parser<bool>`, not `Parser<PEGMatch>`, because they consume no input and have no span to report.

Example 22 in the distribution uses `peg_and` to assert that an identifier begins with a letter, then `peg_many1` to consume the alphanumerics. The same example uses `peg_not` to build a "signed number that is not negative" parser, asserting the absence of a minus sign before consuming digits.

### Bridging Rules and Combinators: `peg_rule`

The `peg_rule` adapter wraps a `PEGRule` in a `Parser<PEGMatch>`. The resulting parser can be composed with the other `peg_*` combinators exactly as if it were a primitive parser. The adapter honours the rule's `must_fail` flag and resolves the match's channel and `ignored` flag from the rule.

```cpp
inline Parser<PEGMatch>
peg_rule (const PEGRule &rule)
{
  return parser ([&rule] (ParsecInput &in) -> ExpectedResult<PEGMatch> {
    std::size_t begin = in.pos;
    auto end = rule.try_match (in.source, begin);
    if (!end)
      return fail_expected<PEGMatch> (in, rule.pattern_text);
    in.pos = *end;
    PEGMatch m;
    m.begin = begin;
    m.end = *end;
    m.value = in.source.substr (begin, *end - begin);
    m.rule = const_cast<PEGRule *> (&rule);
    m.channel = rule.channel;
    m.ignored = rule.channel.is_ignore ();
    peg_detail::line_col_at (in.source, begin, m.line, m.column);
    return m;
  });
}
```

The adapter does *not* fire the rule's stored semantic action. The match record is returned to the caller, who decides what to do with it. This makes `peg_rule` suitable for combinator-style grammars where the actions are attached to the combinator structure rather than to the rules. A grammar that wants both the rule's action and the combinator's structure must call the action explicitly from the combinator's continuation.

The `peg_channel` combinator assigns a channel to a parser's result. It takes a `PEGChannel` (or a channel name string) and a `Parser<T>`, and returns a `Parser<PEGMatch>` whose `channel` and `ignored` fields are set from the supplied channel. This is the combinator-side analogue of setting a rule's `channel` field in the rule-based engine.

## Worked Examples

This section assembles the pieces into running programs. Each example is complete enough to compile with `-std=c++20` against the single `DSLtk.hpp` header.

### Whitespace and Keywords

The first example extends the pattern of example 23 with a derived grammar and a custom channel. The base grammar recognises `if` and `then` as keywords and `[a-zA-Z_][a-zA-Z_0-9]*` as an identifier. Whitespace is on `@IGNORE`. The derived grammar adds a hex literal rule on a custom `@HEX` channel.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

int
main ()
{
  auto grammar = dsl::create_peg_definition ();
  auto &ws = grammar.add_rule<"[ \\t\\n]+"> (
      [] (dsl::PEGMatch &m) { std::cout << "skip_ws:" << m.length () << "\n"; });
  ws.channel = dsl::PEGIgnoreChannel;

  auto &kw_if = grammar.add_rule<"if"> (
      [] (dsl::PEGMatch &m) { std::cout << "kw:" << m.value << "\n"; });
  auto &kw_then = grammar.add_rule<"then"> (
      [] (dsl::PEGMatch &m) { std::cout << "kw:" << m.value << "\n"; });
  auto &ident = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> (
      [] (dsl::PEGMatch &m) { std::cout << "id:" << m.value << "\n"; });

  auto result = grammar.parse ("if flag then");
  std::cout << (result.ok () ? "parse_ok" : "parse_failed") << "\n";
  std::cout << "offset=" << result.offset << "\n";
}
```

The output shows the action firing order: `kw:if`, `skip_ws:1`, `id:flag`, `skip_ws:1`, `kw:then`. The ignore action fires alongside the keyword actions, demonstrating that the channel suppresses token emission but not action invocation. The final `parse_ok` and `offset=11` confirm that the whole input was consumed.

### A Number and Identifier Grammar

The second example uses the combinator family to build a tokenizer that distinguishes signed integers from identifiers. It mirrors example 22 but adds an action that collects tokens into a vector. The `peg_choice` combinator tries the integer rule first; if it matches, the identifier alternative is not tried at that position.

```cpp
#include "DSLtk.hpp"
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

struct Token
{
  std::string kind;
  std::string text;
};

int
main ()
{
  std::vector<Token> tokens;
  auto emit = [&] (std::string kind) {
    return [&] (dsl::PEGMatch &m) {
      tokens.push_back ({ std::move (kind), std::string (m.value) });
    };
  };

  auto digit = dsl::satisfy (
      [] (char c) { return std::isdigit (static_cast<unsigned char> (c)); },
      "digit");
  auto alpha = dsl::satisfy (
      [] (char c) { return std::isalpha (static_cast<unsigned char> (c)); },
      "alpha");
  auto alnum = dsl::satisfy (
      [] (char c) { return std::isalnum (static_cast<unsigned char> (c)); },
      "alnum");

  auto integer = dsl::peg_seq (dsl::peg_opt (dsl::ch ('+') | dsl::ch ('-')),
                               dsl::peg_many1 (digit));
  auto identifier = dsl::peg_seq (dsl::peg_and (alpha),
                                 dsl::peg_many1 (alnum));
  auto token = dsl::peg_choice (integer, identifier);

  auto r = dsl::run_parser (dsl::peg_many (token), "42 abc -7");
  std::cout << "ok=" << (r.value ? "yes" : "no") << "\n";
  for (const auto &t : tokens)
    std::cout << t.kind << ":" << t.text << "\n";
}
```

The `peg_choice` ordering matters: placing `integer` before `identifier` means that a leading digit is parsed as a number, never as the start of an identifier. Reversing the order would parse `42` as an identifier-like token (failing, because `peg_many1(alnum)` accepts digits), changing the token stream. This is the ordered-choice rule in action: the parse is determined by alternative order, not by longest match.

### Ordered Choice and Rule Order

The third example demonstrates ordered choice at the rule level. Two grammars are constructed that differ only in the order of their rules. The first places a keyword rule before the identifier rule; the second reverses them. The same input produces different token streams from the two grammars.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

int
main ()
{
  auto g1 = dsl::create_peg_definition ();
  g1.add_rule<"if"> ([] (dsl::PEGMatch &m) { std::cout << "kw:" << m.value << "\n"; });
  g1.add_rule<"[a-zA-Z_]+"> ([] (dsl::PEGMatch &m) { std::cout << "id:" << m.value << "\n"; });

  auto g2 = dsl::create_peg_definition ();
  g2.add_rule<"[a-zA-Z_]+"> ([] (dsl::PEGMatch &m) { std::cout << "id:" << m.value << "\n"; });
  g2.add_rule<"if"> ([] (dsl::PEGMatch &m) { std::cout << "kw:" << m.value << "\n"; });

  std::cout << "--- g1 ---\n";
  g1.parse ("if");
  std::cout << "--- g2 ---\n";
  g2.parse ("if");
}
```

The first grammar prints `kw:if` because the keyword rule appears first and matches. The second grammar prints `id:if` because the identifier rule appears first and matches the same input. Neither grammar is wrong; they express different intents. The lesson is that PEG rule order is part of the grammar's specification, not an implementation detail.

### Capturing Nested Structure

The fourth example uses the `if_succeed` hook to capture structure that spans more than one rule. A rule matching an opening bracket fires a hook that attempts to match the body and the closing bracket. The hook's action records the inner span; the primary action records the outer span.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

int
main ()
{
  auto grammar = dsl::create_peg_definition ();

  auto &open = grammar.add_rule<"("> (
      [] (dsl::PEGMatch &m) { std::cout << "open@" << m.begin << "\n"; });
  auto &body = grammar.add_rule<"[a-z]+"> (
      [] (dsl::PEGMatch &m) { std::cout << "body:" << m.value << "\n"; });
  auto &close = grammar.add_rule<")"> (
      [] (dsl::PEGMatch &m) { std::cout << "close@" << m.begin << "\n"; });
  auto &ws = grammar.add_rule<"[ \\t]*"> ([] (dsl::PEGMatch &) {});
  ws.channel = dsl::PEGIgnoreChannel;

  auto result = grammar.parse ("(abc)");
  std::cout << (result.ok () ? "ok" : "fail") << "\n";
}
```

This example does not use `if_succeed` directly; instead it relies on the rule list itself expressing the sequence `(`, body, `)` through repeated application of ordered choice at successive positions. The driver advances the cursor after each match, so the next rule is tried at the next position. This is the token-stream model: the grammar describes individual tokens, and structure emerges from their order in the stream.

For genuinely nested structure, the combinator family is more expressive. `peg_seq(open, peg_many(body), close)` parses a bracketed group in one combinator expression, and `peg_many` of that expression parses nested groups. The rule-based engine does not directly express nesting because each rule is applied independently at the current position; nesting requires the combinator layer.

## Pitfalls

### Ordered Choice Is Not Longest Match

PEG ordered choice takes the first alternative that matches, not the longest. A grammar that places a shorter alternative before a longer one will always take the shorter, even when the longer would have consumed more input. This is the most common source of surprise for authors coming from regex or from longest-match parser generators.

The classic example is a keyword rule placed before an identifier rule. The keyword rule matches `if` and the cursor advances by two; the identifier rule, which would have matched `if` as well, is never tried. This is usually the desired behaviour for keywords. The failure mode appears when the shorter rule matches a prefix of a longer token: a keyword `in` placed before an identifier rule will match the `in` of `integer`, leaving `teger` to be parsed by the next rule. The fix is to make the keyword rule require a non-identifier follower, using a negated lookahead or a boundary class.

### Left Recursion Is Not Supported

A PEG rule that begins with a reference to itself, directly or indirectly, will loop forever because the engine always tries the rule at the same position before consuming any input. The library does not implement left-recursion elimination. A grammar that needs left recursion must be rewritten to use right recursion or an explicit loop.

In the rule-based engine, left recursion manifests as an infinite loop in `parse`, because the cursor never advances. In the combinator engine, it manifests as a stack overflow when the parser recurses without consuming input. Neither engine detects the situation; the author must structure the grammar to avoid it.

The standard rewrite for left-recursive rules like `E -> E '+' T | T` is to factor the recursion into a repetition: `E -> T ('+' T)*`. The `peg_many` combinator expresses this directly. The rule-based engine expresses it through repeated application of ordered choice at successive positions, which is equivalent to a repetition when the grammar is structured as a flat token stream.

### Ignore Rules Must Be Ordered Correctly

An ignore rule that can match the same input as a semantic rule will shadow the semantic rule if it appears first in the rule list. Because `skip_ignored` in the `PEGMatcher` runs before the arm list, ignore rules always run first in the matcher path; this is correct behaviour for whitespace. In the `parse` driver, ignore rules run in their declaration order alongside semantic rules, so an ignore rule placed before a semantic rule that matches the same input will suppress the semantic match.

The fix is to make ignore rules specific: a whitespace rule `[ \t\n]+` does not overlap with an identifier rule `[a-zA-Z_]+`, so their relative order does not matter. An ignore rule that matches a broad class like `.*` would shadow everything and is almost certainly a bug.

### Empty-Match Rules Loop Forever Under Repetition

A rule that can match the empty string, placed under `peg_many` or repeated by the parse driver, will loop forever because the cursor does not advance. The parse driver detects this and returns an explicit error. The combinator engine does not detect it; `peg_many(p)` where `p` can match the empty string will loop until the stack overflows.

The fix is to ensure that every repetition consumes at least one character on each iteration. A pattern like `[a-z]*` should be `[a-z]+` when used as a repetition body. A pattern that genuinely matches the empty string, such as an optional clause inside a repetition, should be restructured so that the repetition body cannot succeed without consuming input.

The `peg_many1` combinator enforces this for its first iteration by requiring at least one match. The `peg_many` combinator does not, because zero iterations is a valid result for `a*`. Both assume that the body, when it does match, consumes input; this is the author's responsibility.

## Summary

The PEG engine provides two execution paths over the same grammar. `PEGDefinition::parse` is the all-in-one driver: it walks the input, applies ordered choice across all visible rules at each position, fires actions, and reports success or failure with line and column information. The `PEGMatcher` is the fluent path: it advances one arm at a time, supports wildcard recovery, and offers channel filtering for selective token enumeration. Both paths share `PEGMatch` as their reporting record and both honour the channel discipline that routes ignore matches silently while still firing their actions.

The combinator family `peg_seq`, `peg_choice`, `peg_opt`, `peg_many`, `peg_many1`, `peg_and`, and `peg_not` re-expresses the same PEG semantics through the `dsl::Parser` machinery. The `peg_rule` and `peg_channel` adapters bridge the rule-based and combinator-based worlds, allowing a grammar to mix compiled pattern rules with combinator expressions in a single program. The combinators implement strict PEG semantics: ordered choice with full backtracking on partial failure, greedy repetition that never commits on partial input, and zero-width lookahead for context-sensitive assertions.

The engine's semantics are unforgiving in the ways that matter. Ordered choice is first-match, not longest-match. Left recursion is not supported. Empty-match rules under repetition are detected and rejected by the parse driver, but the combinator engine trusts the author to avoid them. Ignore rules must be specific enough not to shadow semantic rules. These constraints are the price of the engine's simplicity and predictability; a grammar that respects them parses deterministically and reports its failures precisely.

## Looking Back: The Whole Toolkit

This chapter closes the 28-chapter tour of DSLtk. The toolkit began in Chapter 1 with a design philosophy: embedded DSLs expressed as C++20 libraries, header-only, namespace-`dsl`, built on the CRTP foundation of Chapter 3. It proceeded through compile-time strings (Chapter 4), feature tags and mixins (Chapter 5), the pipeline and operator features (Chapters 6 and 7), and the pattern-matching and compile-time regex facilities of Chapters 8 and 9. The pattern regex of Chapter 9 reappears here as the compiled-pattern engine inside `PEGRule`, the same regex subset powering both the static `dsl::pattern<>` matcher and the runtime PEG rules.

The AST layer of Chapters 10 through 14 provided the value type, the tree builders, the traversal and dumping machinery, and the rewrite engine with its fixpoint optimisation. The expression templates of Chapters 15 and 16 made trees lazy and fused their evaluation. Custom literals (Chapter 17), memoization (Chapter 18), `Lazy<T>` (Chapter 19), `Maybe<T>` (Chapter 20), the monadic optional helpers (Chapter 21), and `Result<T,E>` (Chapter 22) supplied the supporting infrastructure that makes embedded DSLs ergonomic and safe.

The parser layer of Chapters 23 through 25 introduced the combinator framework, the primitive parsers, the diagnostic system, and the `run_parser` driver. Chapters 26 and 27 moved from parsing into task pipelines and the PEG definition grammar. This chapter unified the two: the PEG engine's rule-based path and the combinator-based path share the same `PEGMatch` record, the same channel discipline, and the same PEG semantics. A grammar author can choose the rule-based path for its runtime flexibility and the combinator path for its static composability, or mix the two through the `peg_rule` adapter.

The thread that runs through all 28 chapters is the toolkit's commitment to composition. Features compose through mixins. Parsers compose through combinators. Patterns compose through the regex subset. PEG rules compose through ordered choice and the `if_succeed` hook. The same `PEGMatch` record serves both the rule-based and combinator-based PEG engines. The same `dsl::Parser` machinery serves both the standalone parser combinators and the `peg_*` family. A DSL built on DSLtk is itself a composition of these facilities, assembled into a header that reads like a grammar and compiles like a C++ program.
