# Chapter 9: The `pattern<>` Compile-Time Regex Engine

Chapter 8 introduced the `dsl::match` / `dsl::when` / `dsl::otherwise` dispatch machinery and showed that a `when<>` clause can carry a `dsl::pattern<"...">` key in addition to the more conventional value-based keys. That chapter treated the pattern key as an opaque token: something with a `matches()` member that the dispatch table invokes behind the scenes. This chapter opens the box. It documents the `pattern<S>` template in full, explains the matching algorithm it implements, and shows the precise regex subset it supports.

`pattern<S>` is a compile-time, regex-like matcher intended for short classification tasks: distinguishing an integer literal from an identifier, recognising a keyword, or routing a token kind. It is not a general regular-expression engine. It deliberately implements only a small, well-defined subset of regex syntax, just enough to express the character classes and quantifiers that show up in lexical micro-grammars. For richer recognition the library provides the PEG engine described in Chapters 27 and 28, which supports sequencing, alternation, and recursive grammars. This chapter is concerned solely with the lightweight `pattern<>` facility.

The design goal is to keep the matcher small enough to live entirely in a header, to be `constexpr`-evaluable, and to require no allocation. Every match runs against `std::string_view` inputs and produces a `bool`. There is no capture, no grouping, and no state carried between calls. The matcher is a pure function of two strings: the pattern, baked into the type at compile time, and the input, supplied at the call site.

## The `pattern<S>` Template

`pattern` is a class template parameterised by a non-type template parameter of type `dsl::FixedString`. The `FixedString` NTTP (Chapter 4) is what makes it possible to write `dsl::pattern<"[0-9]+">` directly at the source level: the string literal is converted into a compile-time constant that can be carried as a template argument. The pattern text is therefore fixed for the lifetime of the type. Two distinct pattern strings denote two distinct types, and the compiler is free to instantiate each exactly once.

The public surface of the template is intentionally tiny. It exposes two `static constexpr` members: a `value` of type `std::string_view` that mirrors the pattern text, and a `matches()` function that tests whether a given input satisfies the pattern. Everything else is private. The following block shows the template definition in the form a user would reproduce in a manual page.

```cpp
namespace dsl {

template <FixedString S> struct pattern
{
  static constexpr std::string_view value = S.view ();

  static constexpr bool matches (std::string_view input)
  {
    return match_here (S.view (), input);
  }

  // ... private helpers ...
};

} // namespace dsl
```

The `value` member is provided as a convenience for diagnostics, logging, and table construction. It is the textual form of the pattern, recovered from the `FixedString` NTTP via `S.view()`. Because it is `static constexpr`, taking its address or binding it to a `string_view` parameter costs nothing at run time. The `matches()` member is the operational interface. It delegates to the private recursive matcher `match_here`, passing both the pattern text and the input. The recursion is the heart of the engine and is examined in detail below.

Because both members are `static constexpr`, the matcher can be used in `if constexpr` branches, in `consteval` functions, and in any other context that demands a compile-time result. The example shipped as `examples/04-pattern-classes.cpp` uses it from a runtime lambda, but the same calls are equally valid inside a `constexpr` context. The matcher performs no heap allocation, no threading, and no I/O; it is a pure function over two `string_view` ranges.

## Anchored Full-Match Semantics

The most important semantic fact about `pattern<...>::matches()` is that the match is anchored at both ends. A pattern matches an input if and only if the entire input, from the first character to the last, is described by the pattern. There is no notion of a partial or substring match. This is the same convention used by `std::regex_match` rather than `std::regex_search`. Users coming from ECMAScript regex semantics, where matches default to searching, should note the difference.

The anchoring has two practical consequences. First, a pattern such as `"[0-9]+"` does not match the input `"abc123"`; it matches `"123"` but not a string that merely contains `"123"`. Second, the engine does not need a `^` or `$` anchor to express full matching, because full matching is the default. The characters `^` and `$` are accepted by the parser but are silently ignored: they are convenience tokens for users who like to write anchors explicitly, not operational directives.

This anchoring decision is what makes `pattern<>` usable as a classification predicate in a `dsl::match` table. A `when<dsl::pattern<"[0-9]+">>` clause asserts that the *entire* key matches the integer pattern, not merely that the key contains an integer somewhere. That gives dispatch a precise, total-predicate semantics: for any given input key, at most one pattern in a well-formed table will match, provided the patterns are mutually exclusive. The engine itself does not enforce mutual exclusivity, but the anchoring makes it straightforward to achieve by construction.

The decision also keeps the matcher small. A search-style matcher would have to iterate over every starting position in the input and retry the pattern at each; an anchored matcher only has to walk the input once, recursing through the pattern. The implementation analysed below is therefore linear in the combined size of pattern and input for the common cases, and at worst quadratic in pathological backtracking cases involving `*`.

## Supported Syntax Subset

The engine recognises a deliberately small grammar. The supported tokens are: the wildcard `.`, which matches any single character; a character class `[abc]`, which matches any one of the listed characters; a range `[a-z]`, which appears inside a class and matches any character in the inclusive range; the postfix quantifier `*`, meaning zero or more of the preceding atom; and the postfix quantifier `+`, meaning one or more of the preceding atom. The anchors `^` and `$` are parsed and discarded.

Each of these tokens is documented in the comment block attached to the `matches()` member, which is reproduced here for reference.

```cpp
  /**
   * @brief Minimal regex-like match: anchored prefix/suffix, literal chars,
   * [class], *, +.
   *
   * Supported syntax (subset):
   *   .       any character
   *   [abc]   character class
   *   [a-z]   range in class
   *   *       zero or more of previous
   *   +       one or more of previous
   *   ^       (ignored, match is always anchored at start)
   *   $       (ignored, match is always anchored at end)
   *
   * @param input String to test.
   * @return true if the entire input matches the pattern.
   */
```

Any character that is not one of these special tokens is treated as a literal. The literal `'a'` matches the character `'a'` and nothing else. Literals are case-sensitive; the engine has no case-insensitive mode. Because there is no escape mechanism, the characters `.` `[` `]` `*` `+` `^` `$` `-` cannot be matched as literals when they appear in a position where the parser would interpret them as operators. This is one of the trade-offs of keeping the grammar small; users who need to match these characters literally should use the PEG engine (Chapter 27) instead.

The quantifiers `*` and `+` apply to the immediately preceding atom. An atom is either a single literal character, the `.` wildcard, or a complete `[...]` class. Quantifiers do not compose: `[a-z]*+` is not meaningful and is not supported. There is no `?` quantifier, no `{n,m}` counted repetition, and no grouping construct that would allow a quantifier to apply to a sub-expression of more than one atom.

## The `match_class` Helper

Character classes are handled by a dedicated helper, `match_class`. The helper takes two arguments: the body of the class (the text between the brackets, with the brackets themselves stripped), and a single candidate character. It returns `true` if the candidate matches any element of the class, and `false` otherwise. The body is scanned left to right, and the function returns on the first match.

The scan alternates between two interpretations of the body. At each position it checks whether the next three characters form a range, that is, whether the character two positions ahead is a hyphen. If so, the candidate is tested against the inclusive range from the current character to the character after the hyphen, and the scan advances by three. Otherwise the current character is treated as a singleton and the scan advances by one. The following definition is the verbatim form found in the header.

```cpp
  static constexpr bool
  match_class (std::string_view cls, char c)
  {
    for (std::size_t i = 0; i < cls.size ();)
      {
        if (i + 2 < cls.size () && cls[i + 1] == '-')
          {
            if (c >= cls[i] && c <= cls[i + 2])
              return true;
            i += 3;
          }
        else
          {
            if (cls[i] == c)
              return true;
            ++i;
          }
      }
    return false;
  }
```

A range is recognised by the presence of a hyphen at offset `i + 1`, with a character available at offset `i + 2`. The check `i + 2 < cls.size()` guards against a trailing hyphen at the end of the class body; in that case the hyphen is treated as a literal singleton because the range form is not available. This is why a pattern such as `[-a]` matches either a hyphen or the letter `a`, and why `[a-]` matches either an `a` or a hyphen. The behaviour falls out of the index test, not from a special case.

Classes may mix singletons and ranges freely. The pattern `[a-zA-Z_]` matches any ASCII letter, upper or lower case, or an underscore. The pattern `[0-9.]` matches a digit or a full stop; here the `.` inside a class is a literal, because `match_class` does not interpret it as a wildcard. Inside a class, every non-hyphen character is literal. This is a useful rule to remember: the wildcard meaning of `.` applies only outside classes.

The class engine has no notion of negation. There is no `[^...]` form. A class always asserts membership, never exclusion. Users who need to recognise "anything except a digit" must phrase the pattern differently, typically by listing the permitted characters explicitly or by structuring the surrounding `dsl::match` table so that an earlier clause consumes the digit case and a later clause handles the remainder.

## The `match_one` Helper

Once the matcher has decided which atom is next in the pattern, it needs a uniform way to test whether a given input character satisfies that atom. That is the role of `match_one`. The helper takes three arguments: a `char` indicating the kind of atom (`'['` for a class, `'.'` for the wildcard, or any other character for a literal), the class body `cls` (used only when the first argument is `'['`), and the candidate character `c`. It returns `true` if the candidate satisfies the atom.

```cpp
  static constexpr bool
  match_one (char pat, std::string_view cls, char c)
  {
    if (pat == '[')
      return match_class (cls, c);
    if (pat == '.')
      return true;
    return pat == c;
  }
```

The three cases are mutually exclusive and are tested in order. A class atom delegates to `match_class` with the precomputed body. The wildcard atom unconditionally returns `true`, because `.` matches any character, including newline or NUL should either appear in the input. A literal atom performs a single character comparison. The helper is small enough to be inlined trivially, and the compiler typically folds the dispatch into the surrounding recursion.

The reason `match_one` takes both the atom kind and the class body as separate arguments, rather than taking the pattern substring directly, is that the surrounding `match_here` has already parsed the class once and stored the body in a local `cls` variable. Passing the body forward avoids re-parsing the class on every recursive call. This is a small but real efficiency, and it keeps the recursion shallow in terms of work per frame.

The atom-kind parameter uses `'['` as a sentinel for "this is a class". This is safe because a literal `'['` cannot otherwise reach `match_one`: the parser in `match_here` intercepts `'['` and rewrites it into the class path before the atom is tested. Similarly, `'.'` is intercepted only in the wildcard sense; a literal dot would require an escape, which the engine does not provide.

## The `match_here` Recursion

The core of the engine is `match_here`. It takes two `std::string_view` arguments: the remaining pattern and the remaining input. It returns `true` if the remaining input is fully consumed by the remaining pattern. The function is recursive, with one recursive call per atom in the pattern, plus additional recursive calls in the quantifier branches. The verbatim definition is reproduced below; the remainder of this section walks through it clause by clause.

```cpp
  static constexpr bool
  match_here (std::string_view pat, std::string_view str)
  {
    if (pat.empty ())
      return str.empty ();
    if (pat[0] == '^')
      return match_here (pat.substr (1), str);

    // Parse class [...]
    std::string_view cls;
    std::size_t pat_advance = 1;
    char first = pat[0];
    if (first == '[')
      {
        auto end = pat.find (']', 1);
        if (end == std::string_view::npos)
          return false;
        cls = pat.substr (1, end - 1);
        pat_advance = end + 1;
      }

    // Check for quantifier
    char quant = 0;
    if (pat_advance < pat.size ()
        && (pat[pat_advance] == '*' || pat[pat_advance] == '+'))
      quant = pat[pat_advance++];

    auto rest = pat.substr (pat_advance);

    if (quant == '*')
      {
        // Try matching zero occurrences first, then one or more
        if (match_here (rest, str))
          return true;
        while (!str.empty () && match_one (first, cls, str[0]))
          {
            str.remove_prefix (1);
            if (match_here (rest, str))
              return true;
          }
        return false;
      }
    if (quant == '+')
      {
        // One or more
        if (str.empty () || !match_one (first, cls, str[0]))
          return false;
        str.remove_prefix (1);
        while (!str.empty () && match_one (first, cls, str[0]))
          str.remove_prefix (1);
        return match_here (rest, str);
      }

    // Exactly one
    if (str.empty () || !match_one (first, cls, str[0]))
      return false;
    return match_here (rest, str.substr (1));
  }
```

The function opens with two base cases. If the pattern is empty, the match succeeds precisely when the input is also empty; this is what enforces end-anchoring. If the pattern begins with `^`, the anchor is discarded and the function recurses on the remainder. There is no explicit `$` handling: a trailing `$` is simply consumed as a literal character that the input is then required to contain, which is consistent with the documented behaviour that `$` is ignored. In practice, `$` is most useful at the end of a pattern, where it acts as a no-op literal that the input must also contain.

After the base cases, the function parses the current atom. It records the first character of the pattern in `first` and sets `pat_advance` to one, the default advance for a single-character atom. If that first character is `[`, it searches for the matching `]` starting at position one. If no `]` is found the pattern is malformed and the function returns `false`. Otherwise the class body is captured into `cls` and `pat_advance` is advanced to the position just past the `]`. This parsing happens once per atom, not once per character, which is the efficiency motivation mentioned earlier.

With the atom parsed, the function checks for a trailing quantifier. It looks at the character immediately after the atom, which sits at index `pat_advance`. If that character is `*` or `+`, it is recorded in `quant` and `pat_advance` is advanced past it. The variable `rest` is then bound to the pattern suffix that follows the atom (and its quantifier, if any). At this point the function has decomposed the pattern into three parts: the current atom (`first`, optionally with `cls`), an optional quantifier (`quant`), and the rest of the pattern (`rest`).

## The `*` Branch

The `*` quantifier means "zero or more of the preceding atom". The implementation first tries the zero case: it recurses on `rest` with the input unchanged. If that succeeds, the match is done and the function returns `true` immediately. This is the backtrack-friendly way of expressing that the atom is allowed to match nothing.

If the zero case fails, the function enters a `while` loop that consumes input characters one at a time, provided each satisfies the atom via `match_one`. After each successful consumption it recurses on `rest` with the shortened input. As soon as a recursion succeeds, the function returns `true`. If the loop exhausts either the input or the matching characters without a successful recursion, the function returns `false`.

This loop implements greedy matching with backtracking. Greedy, because the loop consumes as many matching characters as it can before each recursive check. Backtracking, because the recursive check is made after every single consumption, not only after the maximal consumption. If consuming three characters allows `rest` to match but consuming four would not, the loop finds the three-character split on its way up and returns `true` at that point. The first recursion that succeeds wins, which means the match is the leftmost-longest consistent with the rest of the pattern.

The cost of this strategy is worth noting. In the worst case, where `*` is followed by a pattern that itself fails late, the loop may retry the recursion many times. For the small patterns and short inputs that `pattern<>` is designed for, this cost is negligible. For pathological inputs the behaviour is quadratic, which is another reason the engine is not intended as a general-purpose regex implementation.

The order of the two strategies matters. Trying the zero case first means that an empty match is preferred whenever it allows the rest of the pattern to succeed. For a pattern such as `[a-z]*x` against the input `"x"`, the engine first tries matching `[a-z]*` against the empty prefix, which leaves `x` to match the literal `x`, and succeeds immediately. Trying the greedy case first would have consumed the `x` and then failed to match the trailing literal, requiring a backtrack. The zero-first ordering avoids that backtrack in this common case.

## The `+` Branch

The `+` quantifier means "one or more of the preceding atom". Its implementation is similar to the `*` branch but with the zero case removed. The function first requires that the input is non-empty and that its first character satisfies the atom; if not, the match fails immediately. This is the difference between `+` and `*`: `+` demands at least one match, `*` permits none.

Having consumed the first character, the function enters a `while` loop that consumes further matching characters without performing intermediate recursion. This is the greedy phase: it eats as many matching characters as possible. Only when the loop terminates (because the input is exhausted or the next character fails the atom) does the function recurse on `rest` with the shortened input. The result of that recursion is returned directly.

This is a notable asymmetry with the `*` branch. The `*` branch recurses after every consumption, because it must be willing to backtrack to a shorter match. The `+` branch recurses only once, after maximal consumption, because it has already guaranteed at least one match and there is no shorter match that could satisfy `+` while failing the maximal one. In other words, `+` is greedily maximal and does not backtrack within itself; the recursion it performs at the end is the only point at which the rest of the pattern gets to vote.

This means that a pattern such as `[0-9]+9` against the input `"99"` will fail. The `+` consumes both nines greedily, leaving the rest of the pattern `9` to match against the empty input, which fails. There is no backtrack to a one-character `+` match. Users should be aware of this when stacking quantifiers next to literals that overlap the quantified class. The behaviour is a deliberate trade-off favouring simplicity and speed over the full backtracking semantics of ECMAScript `+`.

For the common lexical-classification patterns, this restriction is invisible. `[0-9]+` against `"123"` consumes all three digits and then recurses on the empty pattern with the empty input, which succeeds. `[a-zA-Z_][a-zA-Z_0-9]*` consumes an initial letter or underscore and then as many word characters as possible, again terminating cleanly. The restriction only bites when a literal follows a quantifier and that literal could itself be consumed by the quantified atom.

## The Exactly-One Fallthrough

If no quantifier is present, the function falls through to the exactly-one case. This is the simplest path. It requires that the input is non-empty and that its first character satisfies the atom via `match_one`. If so, it recurses on `rest` with the input shortened by one character. If not, it returns `false`.

This is the path taken by literal characters, by `.` wildcards without a quantifier, and by character classes without a quantifier. The recursion advances the pattern by one atom and the input by one character, maintaining the invariant that the remaining input must be exactly what the remaining pattern describes. The base case at the top of the function — empty pattern implies empty input — terminates the recursion when all atoms have been consumed.

Because each recursive call in this branch advances the pattern by exactly one atom, the depth of recursion is bounded by the number of atoms in the pattern. For a pattern of length N the recursion depth is at most N. This makes the exactly-one path predictable and cheap, and it is the path that the overwhelming majority of atoms in a typical pattern take.

The exactly-one path is also responsible for handling `$` if it appears at the end of a pattern without a quantifier. A pattern such as `[0-9]+$` is parsed as: a class atom `[0-9]`, followed by `+`, followed by a literal `$`. The `$` is matched as a literal character that the input must contain. This is rarely what the user intends, which is why the documentation describes `$` as "ignored": the practical way to use `$` is to omit it, since matching is end-anchored by construction.

## Greedy Semantics and Backtracking

The interaction between the `*` and `+` branches and the rest of the pattern determines the engine's greedy semantics. Both quantifiers consume as much input as they can. The `*` branch then backtracks character by character, retrying the rest of the pattern at each shorter length, until it finds a split that lets the rest succeed. The `+` branch does not backtrack within itself; it commits to its maximal consumption and delegates to the rest of the pattern exactly once.

For most classification patterns, the rest of the pattern is empty or a fixed literal, and the distinction is invisible. The interesting cases arise when a quantifier is followed by another atom that overlaps the quantified class. Consider the pattern `[a-z]*z` against the input `"abz"`. The `*` branch first tries the zero case: matching `z` against `"abz"` fails, because the first character is `a`. The loop then consumes `a`, retries `z` against `"bz"` (fails), consumes `b`, retries `z` against `"z"` (succeeds), and returns `true`. The engine finds the split because of the per-consumption recursion in the `*` branch.

The same pattern against `"abzc"` would fail, because the `*` branch would consume `a`, `b`, `z`, and then retry `z` against `"c"` (fails), exhaust the input, and return `false`. There is no mechanism to try a different split of the `*` consumption, because the loop only shortens by failing to match the next character; it cannot re-consume a character it has already passed. This is consistent with the leftmost-greedy model and is the same behaviour a user would obtain from a simple hand-rolled backtracking matcher.

The takeaway for users is that `pattern<>` implements a small, predictable subset of regex backtracking. It is sufficient for the lexical-classification tasks it was designed for, and it behaves consistently across all inputs. Where richer semantics are required — non-greedy quantifiers, alternation, grouping — the PEG engine in Chapters 27 and 28 is the appropriate tool.

## Integration with `when<>` and `dsl::match`

The `pattern<>` template is designed to be used as the key of a `when<>` clause. Chapter 8 covered the consumer side in detail; this section covers the producer side, that is, what `WhenClause::try_invoke` does when it encounters a pattern key. The relevant code is the `requires` check at the top of `try_invoke`.

```cpp
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
```

The detection is structural rather than nominal. `try_invoke` does not ask whether `KeyT` is a `dsl::pattern<S>` instantiation; it asks whether `KeyT` exposes a `matches(std::string_view)` member. Any type that satisfies this requirement is treated as a pattern key. This is a deliberate design choice: it allows users to plug in their own matcher types, provided they expose the same `static constexpr bool matches(std::string_view)` shape. The `dsl::pattern<>` template is the canonical such type, but it is not the only one the dispatch machinery will accept.

When the `requires` clause is satisfied, `try_invoke` further requires that the runtime key be convertible to `std::string_view`. This is what makes pattern-based dispatch usable with `const char*`, `std::string`, `std::string_view`, and any other string-like type. The key is converted to a `string_view` and passed to `KeyT::matches()`. If the match succeeds, the handler is invoked with the original key (not the converted view) and any trailing arguments, and the clause returns `true`. If the match fails, the clause returns `false` and the dispatch table proceeds to the next clause.

This routing is what allows a single `dsl::match` table to mix value-based and pattern-based clauses freely. Value-based clauses use the `else if` branch with `equality_comparable_with`; pattern-based clauses use the `if` branch with `requires`. The two are mutually exclusive at compile time: a key type either has a `matches()` member or it does not. There is no ambiguity, and there is no run-time cost to the dispatch, because the `if constexpr` discards the unused branch at compile time.

## A First Example: Classifying Integers and Words

The simplest use of `pattern<>` is direct invocation of `matches()` from a lambda or function. The example shipped as `examples/04-pattern-classes.cpp` shows the canonical pattern: a small lambda that tests an input against several patterns in sequence and returns a label.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto classify = [](std::string_view s) {
    if (dsl::pattern<"[0-9]+">::matches(s)) return "int";
    if (dsl::pattern<"[a-z]+">::matches(s)) return "word";
    return "other";
  };
  std::cout << classify("123") << " " << classify("abc") << "\n";
}
```

The patterns here are mutually exclusive by construction. A string of digits satisfies `[0-9]+` and not `[a-z]+`. A string of lowercase letters satisfies `[a-z]+` and not `[0-9]+`. A string containing any other character, such as `"a1"` or `"@x"`, satisfies neither and falls through to the `"other"` return. Because matching is anchored, `"a1"` does not match `[a-z]+` even though it begins with letters; the entire input must be described by the pattern.

The order of the tests matters. A string such as `"123"` matches the first pattern and short-circuits; the second test is never run. Reordering the tests would change the result for inputs that could match more than one pattern, although in this particular example the patterns are disjoint. In general, when patterns overlap, the first matching clause wins, exactly as in a `switch` statement with a `default`.

This style of classification is appropriate when the set of categories is small and fixed. For larger or more dynamic taxonomies, embedding the patterns in a `dsl::match` table is preferable, because it lets the dispatch machinery handle the ordering and fallthrough uniformly. That style is shown below in the discussion of example 19.

## Identifiers and Keywords

A more realistic lexical classification distinguishes identifiers, keywords, and numbers. Identifiers in most C-like languages match the pattern `[a-zA-Z_][a-zA-Z_0-9]*`: an initial letter or underscore, followed by zero or more letters, underscores, or digits. Keywords are a fixed set of literal strings. Numbers match `[0-9]+`. The example shipped as `examples/15-tokenize-parse-integration.cpp` combines all three.

```cpp
  auto classify_by_key = [](std::string_view token) {
    if (dsl::pattern<"[0-9]+">::matches (token))
      return LexemeClass::Number;
    if (dsl::pattern<"if">::matches (token)
        || dsl::pattern<"for">::matches (token)
        || dsl::pattern<"while">::matches (token))
      return LexemeClass::Keyword;
    if (dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches (token))
      return LexemeClass::Identifier;
    return LexemeClass::Other;
  };
```

The number test comes first because it is the cheapest and the most specific. The keyword test comes second and consists of three literal patterns combined by `||`. Each literal pattern is a `pattern<"...">` instantiation with a fixed string; `matches()` returns `true` only when the input is exactly that string, because of the end-anchoring. The identifier test comes last and is the most general; it would also match the keywords, which is why the keyword test must precede it.

The identifier pattern is a good illustration of how atoms and quantifiers compose in this engine. The first atom is the class `[a-zA-Z_]`, which matches one letter or underscore. The second atom is the class `[a-zA-Z_0-9]` followed by the `*` quantifier, which matches zero or more word characters. The engine parses this as two atoms: a single class, and a class-with-quantifier. There is no grouping construct, but none is needed, because the quantifier already applies to the immediately preceding atom, which is exactly the second class.

The keyword test uses three separate `pattern<>` instantiations rather than a single pattern with alternation. This is because the engine does not support the `|` operator. Users who need alternation within a single pattern must either enumerate the alternatives as separate `pattern<>` instantiations combined by `||`, as shown here, or move to the PEG engine. For small fixed keyword sets the enumeration approach is clear and efficient, and it has the added benefit of making each keyword a distinct type, which can be useful for diagnostics.

## Tokenizer-Kind Classification

The example shipped as `examples/19-pattern-match.cpp` shows the same classification idea in an even more compact form, returning string labels rather than enum values.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto token_kind = [](std::string_view t) {
    if (dsl::pattern<"[0-9]+">::matches(t)) return "INT";
    if (dsl::pattern<"[a-z]+">::matches(t)) return "ID";
    return "UNK";
  };
  std::cout << token_kind("abc") << " " << token_kind("99") << "\n";
}
```

This example is deliberately minimal. It shows that `pattern<>` is usable as a one-liner predicate, with no setup, no state, and no dependencies beyond the header itself. The two patterns are disjoint and the fallthrough case returns a generic label. The example is a useful starting point for users who want to experiment with the matcher before integrating it into a larger dispatch table.

The simplicity of this example also highlights a limitation. Because the engine does not support `\d` or other shorthand classes, every classification pattern must spell out its character ranges explicitly. A pattern such as `[a-z]+` will not match uppercase letters, digits, or underscores; if those should be classified as identifiers, the pattern must be widened, as in the `[a-zA-Z_][a-zA-Z_0-9]*` form used in the previous example. Choosing the right pattern is a matter of understanding the lexical rules of the language being classified.

## Patterns Inside a `dsl::match` Table

The most powerful use of `pattern<>` is as a key inside a `dsl::match` table. In this style, the dispatch machinery itself runs the pattern tests, and the user supplies only the handlers. The example from `examples/15-tokenize-parse-integration.cpp` continues by defining a dispatch table over the `LexemeClass` enum, where the classification has already been performed by the lambda shown above. The table itself uses value-based clauses for the enum, but the same machinery supports pattern-based clauses directly.

```cpp
  auto dispatch = dsl::match (
      dsl::when<LexemeClass::Number> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":number(" + std::string (token) + ")"; }),
      dsl::when<LexemeClass::Keyword> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":keyword(" + std::string (token) + ")"; }),
      dsl::when<LexemeClass::Identifier> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":identifier(" + std::string (token) + ")"; }),
      dsl::otherwise (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":other(" + std::string (token) + ")"; }));
```

In this example the pattern-based classification and the value-based dispatch are kept separate, with the classification lambda producing an enum that the table then consumes. This separation is a matter of style. A more compact design would place the patterns directly in the table, as in the following sketch, which the `requires`-based detection in `try_invoke` fully supports.

```cpp
  auto dispatch = dsl::match (
      dsl::when<dsl::pattern<"[0-9]+">> (
          [] (std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":number(" + std::string (token) + ")"; }),
      dsl::when<dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">> (
          [] (std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":identifier(" + std::string (token) + ")"; }),
      dsl::otherwise (
          [] (std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":other(" + std::string (token) + ")"; }));
```

Here the table is invoked directly with the token string. The first clause whose pattern matches the token executes its handler; if none match, the `otherwise` clause runs. The order of the clauses is the order of the tests, so a more specific pattern must precede a more general one that would also match the same input. This is the same ordering discipline required by any pattern-matching dispatch, and it is the responsibility of the user, not the engine.

The `requires`-based detection ensures that the `when<dsl::pattern<"...">>` clauses are routed to the `matches()` call, while a `when<some_enum_value>` clause in the same table would be routed to the equality comparison. The two styles can be mixed in a single table, although doing so is unusual because the key types would have to agree across clauses. In practice, a table is either fully value-based or fully pattern-based; the engine supports both uniformly.

## Worked Examples of Pattern Matching

This section presents a series of small examples that illustrate the matching semantics in isolation. Each example shows a pattern, an input, and the result, with a brief explanation of why the engine produces that result. These examples are intended to build a precise intuition for the engine's behaviour.

```cpp
static_assert( dsl::pattern<"[0-9]+">::matches("123"));
static_assert(!dsl::pattern<"[0-9]+">::matches(""));
static_assert(!dsl::pattern<"[0-9]+">::matches("12a"));
static_assert( dsl::pattern<"[0-9]*">::matches(""));
static_assert( dsl::pattern<"[a-z]*">::matches(""));
static_assert( dsl::pattern<"[a-z]*">::matches("hello"));
static_assert(!dsl::pattern<"[a-z]*">::matches("Hello"));
```

The first assertion holds because `[0-9]+` requires at least one digit and the input is all digits. The second fails because `+` requires at least one. The third fails because end-anchoring demands that the entire input be digits, and the trailing `a` breaks the match. The fourth and fifth hold because `*` permits zero matches. The sixth holds because the input is all lowercase letters. The seventh fails because the initial uppercase `H` is not in the class `[a-z]`.

```cpp
static_assert( dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches("alpha_7"));
static_assert( dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches("_"));
static_assert(!dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches("7alpha"));
static_assert( dsl::pattern<".">::matches("x"));
static_assert(!dsl::pattern<".">::matches(""));
static_assert( dsl::pattern<".*">::matches(""));
static_assert( dsl::pattern<".*">::matches("anything"));
```

The identifier pattern matches `"alpha_7"` because the first character is a letter and the rest are word characters. It matches `"_"` because the first character is an underscore and the `*` permits the rest to be empty. It fails to match `"7alpha"` because the first character is a digit, which is not in the initial class. The wildcard examples show that `.` requires exactly one character and `.*` accepts any string, including the empty string.

```cpp
static_assert( dsl::pattern<"if">::matches("if"));
static_assert(!dsl::pattern<"if">::matches("iffy"));
static_assert(!dsl::pattern<"if">::matches("If"));
static_assert( dsl::pattern<"^if">::matches("if"));
static_assert( dsl::pattern<"if$">::matches("if"));
```

The literal-pattern examples show end-anchoring in action. `"if"` matches the pattern `"if"` exactly, but `"iffy"` does not because the engine requires the entire input to be consumed. `"If"` does not match because matching is case-sensitive. The `^if` pattern matches because the `^` is discarded and the rest is the literal `if`. The `if$` pattern matches the input `"if"` because the `$` is treated as a literal that the input must contain; since the input is exactly `"if"`, the literal `$` would not in fact be present, and the assertion would fail for an input that did not contain a literal dollar sign. Users should therefore omit `$` rather than rely on it.

The final point about `$` deserves emphasis. The documentation describes `$` as "ignored", but the implementation treats it as a literal character when it appears in a position where a literal would be expected. The practical consequence is that a pattern ending in `$` will only match inputs that literally contain a dollar sign at the corresponding position. To avoid surprises, users should treat `$` as unsupported and omit it entirely; the end-anchoring of the engine already provides the semantic that `$` would express in a conventional regex.

## Limitations of the Engine

The `pattern<>` engine is intentionally limited. This section catalogues the limitations honestly, so that users can decide whether the engine is appropriate for a given task. The limitations are not defects; they are design choices that keep the engine small and predictable. Users who need richer semantics should use the PEG engine described in Chapters 27 and 28.

The engine does not support alternation. There is no `|` operator, and therefore no way to write a single pattern that matches either `"if"` or `"for"`. The workaround is to use multiple `pattern<>` instantiations combined by `||` at the C++ level, as shown in the keyword example above. This is slightly more verbose but has the advantage of making each alternative a distinct type, which can aid diagnostics.

The engine does not support grouping. There are no parentheses, and therefore no way to apply a quantifier to a sub-expression of more than one atom. The pattern `(ab)*` is not expressible. This is a more fundamental restriction than the lack of alternation, because there is no simple workaround at the C++ level. Users who need to recognise repeated multi-character sequences must use the PEG engine.

The engine does not support backreferences. There is no capture, and therefore no way to refer to a previously matched substring later in the pattern. This is consistent with the engine's pure-function design: a match is a function of pattern and input, with no state carried between calls. Backreferences are incompatible with that design.

The engine does not support shorthand classes. There is no `\d`, `\w`, `\s`, or any other backslash escape. Every class must spell out its characters or ranges explicitly. There is also no escape mechanism for matching special characters literally; the characters `.`, `[`, `]`, `*`, `+`, `^`, `$`, and `-` cannot be matched as literals in positions where the parser would interpret them as operators. Inside a class, `-` is the only character with a special meaning, and only when it appears between two other characters.

The engine does not support counted repetition. There is no `{n}`, `{n,}`, or `{n,m}` form. A pattern that requires exactly three digits must be written as `[0-9][0-9][0-9]`. This is verbose but straightforward for small counts, and it is adequate for the lexical-classification tasks the engine is designed for.

The engine does not support the `?` quantifier. There is no zero-or-one operator. A pattern that permits an optional character must be expressed by other means, typically by listing the variants explicitly or by restructuring the surrounding dispatch. The `*` quantifier can sometimes substitute, but it permits more than one occurrence, which is not always acceptable.

Classes cannot nest. The engine finds the first `]` after the opening `[` and treats everything in between as the class body. There is no support for `[[a-z]&&[a-m]]` or any other nested form. The class body is a flat sequence of singletons and ranges. This is sufficient for the character sets that arise in lexical classification, but it precludes more sophisticated class expressions.

Contrasting with full POSIX or ECMAScript regex, the `pattern<>` engine omits: alternation, grouping, backreferences, shorthand classes, counted repetition, non-greedy quantifiers, case-insensitive matching, multiline mode, lookahead, lookbehind, and Unicode character properties. What it provides is a small, fast, compile-time-evaluable matcher for anchored full matches over a tiny but useful regex subset. For tasks within its scope it is adequate and convenient; for tasks beyond its scope the PEG engine is the recommended alternative.

## Choosing Between `pattern<>` and the PEG Engine

The library offers two recognition engines: the `pattern<>` engine described in this chapter, and the PEG engine described in Chapters 27 and 28. The two are complementary, and the choice between them depends on the task. `pattern<>` is appropriate when the recognition task is a single anchored classification predicate, when the pattern is short and fixed, and when the set of categories is small. The PEG engine is appropriate when the recognition task involves sequencing, alternation, or recursion, when the grammar is too complex to express as a single regex, or when the result of recognition is a structured parse tree rather than a boolean.

A useful rule of thumb is to reach for `pattern<>` first, and to move to the PEG engine only when `pattern<>` proves insufficient. The `pattern<>` engine is smaller, simpler, and has no dependencies beyond the header itself. Its matching algorithm is easy to reason about, and its limitations are easy to work around for the kinds of tasks it was designed for. The PEG engine is more powerful but also more complex, and it brings its own concepts of grammar definition, combinator composition, and parse-tree construction.

In a typical DSLtk workflow, `pattern<>` is used in the early stages of a pipeline, where tokens are classified into kinds, and the PEG engine is used in later stages, where the token stream is parsed into a tree. The two engines share no code but coexist peacefully in the same header. A program can use both without conflict, and the choice of engine for a given task is purely a matter of expressiveness and convenience.

## A Note on `constexpr` Evaluability

Every member of `pattern<>` is `static constexpr`, and the helpers it calls are likewise `constexpr`. This means that a match can be performed entirely at compile time, provided the input is a compile-time constant. The `static_assert` examples shown earlier in this chapter are not merely illustrative; they are genuine compile-time tests that the compiler evaluates during template instantiation.

Compile-time matching is useful for asserting invariants about literal strings embedded in a program. For example, a library author might assert that a particular configuration string matches an expected pattern, with the check performed at build time and with no run-time cost. The same machinery can be used in `consteval` functions to construct compile-time lookup tables indexed by pattern.

The cost of compile-time matching is borne by the compiler, and for very long inputs or pathological patterns the compile time can become noticeable. For typical lexical-classification patterns and short inputs the cost is negligible. Users who observe slow compiles should check whether a `pattern<>` instantiation is being invoked repeatedly in a tight `constexpr` context, and consider hoisting the result into a named `constexpr` variable.

At run time, the matcher is a plain function with no virtual dispatch, no allocation, and no exceptions. It can be called from hot loops without concern. The recursion depth is bounded by the pattern length, and the work per recursive call is a small constant. For the inputs the engine is designed for, performance is not a consideration.

## Composing Patterns with Operators

Although `pattern<>` itself has no composition operators, the surrounding `dsl` namespace provides facilities for combining predicates. Chapter 7 covers the operators feature in detail, including the predicate-composition operators that can be used to build compound matchers from `pattern<>` instantiations. The typical pattern is to wrap a `pattern<...>::matches` call in a predicate and then combine predicates with `&&` or `||`.

This style of composition is more verbose than writing a single regex with alternation, but it has the advantage of preserving the type identity of each component pattern. Each `pattern<>` instantiation remains a distinct type, which can be useful for diagnostics and for dispatch. The composition happens at the predicate level, not at the pattern level, and the resulting predicate can be used anywhere a boolean predicate is accepted.

For the simple classification tasks that are the primary use case of `pattern<>`, composition is rarely needed. The patterns are usually disjoint, and a sequence of `if` tests is the clearest expression of the classification. Composition becomes more valuable when patterns overlap in complex ways, or when the same pattern is reused across multiple classifiers. In those cases the predicate-composition operators provide a clean way to build reusable building blocks.

## Diagnostics and the `value` Member

The `value` member of `pattern<>` is provided primarily for diagnostics. Because it is a `static constexpr std::string_view`, it can be included in error messages, log lines, and debug output without any run-time cost. A typical use is in a fallback handler that reports which patterns were tried and failed, to assist the user in understanding why a given input was classified as "other".

```cpp
auto report = [](std::string_view input) {
  if (dsl::pattern<"[0-9]+">::matches(input)) return "number";
  if (dsl::pattern<"[a-z]+">::matches(input)) return "word";
  return "other";
};
// In a diagnostics path:
std::cerr << "pattern value: " << dsl::pattern<"[0-9]+">::value << "\n";
```

The `value` member is also useful when constructing dispatch tables programmatically. A table-builder that accepts `pattern<>` instantiations can expose the pattern text to the user for confirmation, without having to parse the template arguments. This is a small convenience, but it makes the engine slightly easier to use in metaprogramming contexts where the pattern text is not otherwise visible.

Because `value` is a `string_view` into the `FixedString` NTTP, it is valid for the lifetime of the program. There is no dangling-pointer risk, no matter how long the `string_view` is held. This is a consequence of the NTTP storage model: the pattern text lives in static storage for the entire program, and the `string_view` merely points into it.

## Summary

`dsl::pattern<S>` is a small, compile-time-evaluable regex-subset matcher parameterised on a `FixedString` NTTP. It exposes a `value` member for diagnostics and a `matches()` member for testing whether an input satisfies the pattern. Matching is anchored at both ends: the entire input must be described by the pattern. The supported syntax comprises the wildcard `.`, character classes `[abc]`, ranges `[a-z]` within classes, the `*` and `+` postfix quantifiers, and the `^` and `$` anchors (which are accepted but not meaningful). The engine is implemented by three private helpers: `match_class`, which scans a class body for singletons and ranges; `match_one`, which dispatches on the atom kind; and `match_here`, the recursive matcher that parses atoms, applies quantifiers with greedy backtracking semantics, and enforces end-anchoring.

Integration with the dispatch machinery of Chapter 8 is structural: any type exposing a `static constexpr bool matches(std::string_view)` member is treated as a pattern key by `WhenClause::try_invoke`, which converts the runtime key to a `string_view` and delegates to the matcher. This lets `when<dsl::pattern<"...">>` clauses sit alongside value-based clauses in a `dsl::match` table, with the routing decided at compile time by `if constexpr`.

The engine is deliberately limited. It omits alternation, grouping, backreferences, shorthand classes, counted repetition, the `?` quantifier, nested classes, and case-insensitive matching. These omissions keep the engine small and predictable, and they scope it to the lexical-classification tasks for which it was designed. For richer recognition, including sequencing, alternation, and recursive grammars, the PEG engine of Chapters 27 and 28 is the appropriate tool. Within its scope, `pattern<>` provides a fast, allocation-free, `constexpr`-evaluable matcher that is convenient to use and easy to reason about.
