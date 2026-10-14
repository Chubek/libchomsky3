# Chapter 17: Custom Literals: `lit`, `literal_set`, `parse_literal`

A domain-specific language frequently needs to recognise a fixed vocabulary of
named spellings: keywords such as `if`, `then`, and `else`; operators such as
`+`, `-`, and `<=`; or unit suffixes such as `_km` and `_cm` that decorate
numeric literals. The `CustomLiterals` feature of DSLtk provides a small,
compile-time registry for exactly these cases. A DSL author declares a static
table of literal entries, and the toolkit generates a parsing entry point that
classifies an input string against that table at run time.

This chapter describes every component of the feature: the `LiteralEntry`
record, the `lit<>` factory, the `LiteralSet` container and its `literal_set`
variadic factory, the `parse` member that performs the lookup, the
`CustomLiterals` feature tag and its mixin, the `parse_literal` entry point on
the derived DSL, and the `HasLiterals` concept that constrains types
participating in the feature. The chapter also explains the matching semantics
in detail and illustrates the feature with worked examples drawn from unit
conversion, keyword tables, operator tables, and a small token classifier.

The feature is intentionally narrow. It does not implement a general lexer,
nor does it compete with the parser combinators of Chapter 23 or the PEG
machinery of Chapter 27. Instead it offers a compact, typed table that is
useful as a building block inside larger front ends, and as a convenient
adapter for numeric literals that carry a unit suffix. Readers familiar with
the `FixedString` non-type template parameter introduced in Chapter 4 will
recognise the mechanism by which literal spellings are carried at compile time.

## Motivation: Registering Named Spellings

Consider a units DSL that accepts strings such as `3.5_km` and `100_cm`. The
numeric part is parsed with the standard library, while the trailing suffix
selects a conversion factor. Without a registry, the DSL author would hand-write
a chain of `if` statements comparing the tail of the string against each known
suffix. That chain is repetitive, error-prone, and detached from the type
system.

The `CustomLiterals` feature replaces the hand-written chain with a declarative
table. Each row of the table is a `LiteralEntry` that pairs a compile-time
spelling with a run-time handler. The table is aggregated into a `LiteralSet`,
and the set exposes a single `parse` function that scans its rows in order and
returns the result of the first matching handler.

The same machinery serves non-numeric vocabularies. A language with keywords
`if`, `then`, and `else` can register each keyword as a literal whose handler
yields an enumeration tag. A token stream can then be classified by feeding
each token string through the set. The handler type is general; it is not
restricted to numeric conversion.

Because the spelling is a `FixedString` non-type template parameter, it is
fixed at compile time and the compiler can inline the comparison. The set
itself is a value of an aggregate type whose entry list is encoded in its type,
so a `LiteralSet` can be declared `constexpr` and stored as a static member of
a DSL without any run-time initialisation cost.

## The LiteralEntry Record

The unit of registration is the `LiteralEntry` class template. Its definition
in the header reads as follows.

```cpp
template <FixedString Suffix, typename F> struct LiteralEntry
{
  F handler;
  static constexpr std::string_view suffix = Suffix.view ();

  explicit constexpr LiteralEntry (F f) : handler (std::move (f)) {}
};
```

The template has two parameters. The first, `Suffix`, is a `FixedString` (see
Chapter 4) used as a non-type template argument; it carries the spelling of the
literal at compile time. The second, `F`, is the type of a callable that
accepts a `long double` and returns a value of arbitrary type. The handler is
stored by value as the `handler` member.

Each entry exposes its spelling as the static member `suffix`, which is a
`std::string_view` obtained from `Suffix.view()`. This member is what the
`LiteralSet` consults at run time when scanning the table. Because `suffix` is
`static constexpr`, it costs no per-instance storage and the view can be taken
without constructing any `std::string`.

The constructor is `explicit` and `constexpr`, so an entry can be constructed
in a constant expression. The `std::move` on the parameter is benign for
typical lambdas but allows move-only callables to be registered without copy.
Note that the entry itself does no parsing; it merely holds the spelling and
the handler. Parsing is the responsibility of the containing set.

The spelling carried by `Suffix` is interpreted as a suffix of the input
string by the set's `parse` function. This suffix-oriented design is what makes
the feature natural for unit-decorated numerics such as `3.5_km`, where the
interesting lexical structure is the trailing unit. For keyword tables the
spelling is compared against the whole token, which is simply the degenerate
case of a suffix match against a one-token input.

## The lit<> Factory

Constructing a `LiteralEntry` directly requires naming two template arguments:
the `FixedString` spelling and the deduced handler type. The `lit` factory
function deduces the handler type so that the author only writes the spelling
explicitly. Its definition is:

```cpp
template <FixedString Suffix, typename F>
constexpr auto
lit (F &&f)
{
  return LiteralEntry<Suffix, std::decay_t<F>>{ std::forward<F> (f) };
}
```

The spelling is passed as an explicit template argument, writing the literal
text inside angle brackets as a string literal. The handler is passed as an
ordinary forwarding reference, and its decayed type is used as the `F` argument
to `LiteralEntry`. The result is a prvalue of the entry type, suitable for
passing to `literal_set`.

A typical use looks like the following, taken from the header's own
documentation.

```cpp
dsl::lit<"_km">([](long double v){ return v * 1000.0L; })
```

The spelling `"_km"` is a compile-time value. The lambda is the handler; it
receives the numeric prefix of the input and returns the converted value. The
return type of the handler is whatever the lambda yields, here `long double`.
Different entries in the same set may have different handler return types,
although the set's `parse` function normalises the result to `long double` as
described below.

Because `Suffix` is a `FixedString` NTTP, the spelling must be a compile-time
constant. A spelling read from a configuration file at run time cannot be
passed to `lit<>`; it would have to be dispatched on manually. This is a
deliberate restriction that keeps the table static and the comparisons
inlinable.

The factory is `constexpr`, so entries can be assembled during constant
evaluation. This is what allows a DSL to declare its `literals` member as
`static constexpr auto literals = dsl::literal_set(...)` with the entire table
built at compile time and placed in read-only storage.

## The LiteralSet Container

A single entry is rarely useful in isolation. The `LiteralSet` class template
aggregates an arbitrary number of `LiteralEntry` objects into a typed tuple and
provides the `parse` member that scans them. Its definition reads:

```cpp
template <typename... Entries> struct LiteralSet
{
  std::tuple<Entries...> entries;

  explicit constexpr LiteralSet (Entries... es) : entries (std::move (es)...)
  {
  }

  long double
  parse (std::string_view input) const
  {
    long double result = 0.0L;
    bool found = false;

    std::apply (
        [&] (const auto &...e)
          {
            (
                [&]
                  {
                    if (found)
                      return;
                    auto suf = e.suffix;
                    if (input.size () > suf.size ()
                        && input.substr (input.size () - suf.size ()) == suf)
                      {
                        auto num_str
                            = input.substr (0, input.size () - suf.size ());
                        long double num = std::stold (std::string (num_str));
                        result = e.handler (num);
                        found = true;
                      }
                  }(),
                ...);
          },
        entries);

    if (!found)
      {
        throw std::runtime_error (
            std::string ("dsl::LiteralSet::parse: no suffix matched for '")
            + std::string (input) + "'");
      }
    return result;
  }
};
```

The set is parameterised on the entry types, so the full type of the table
encodes the number and order of its rows. Two sets that register the same
spellings in a different order have different types. The entries are stored in
a `std::tuple<Entries...>` member named `entries`, constructed by
perfect-forwarding each argument into the tuple.

The constructor is `explicit` and `constexpr`. The `explicit` qualifier
prevents accidental implicit conversion from a single entry to a one-element
set, which would otherwise be a surprising conversion path. The `constexpr`
qualifier permits the whole table to be assembled during constant evaluation.

The set is an aggregate of sorts: it has no virtual functions, no base
classes, and no invariants beyond the construction of its tuple. It is
therefore cheap to copy and cheap to store as a static member. The
`std::tuple` is the only non-trivial member, and even it is composed entirely
of trivially relocatable callables in typical use.

## The literal_set Variadic Factory

As with `lit`, the toolkit provides a deducing factory so that the author need
not spell the entry types. The `literal_set` factory is:

```cpp
template <typename... Entries>
constexpr auto
literal_set (Entries &&...es)
{
  return LiteralSet<std::decay_t<Entries>...>{ std::forward<Entries> (es)... };
}
```

The factory forwards its arguments into a newly constructed `LiteralSet` whose
type arguments are the decayed types of the arguments. Passing a mixture of
`LiteralEntry` instantiations produced by `lit<>` yields a `LiteralSet` whose
`Entries` pack lists those exact entry types. The result is a prvalue of the
set type and may be bound to a `constexpr auto` variable.

A typical table is therefore written without any explicit type names, as in
the following unit-conversion table drawn from the header examples.

```cpp
constexpr auto lits = dsl::literal_set(
    dsl::lit<"_km">([](long double v){ return v * 1000.0L; }),
    dsl::lit<"_m"> ([](long double v){ return v; }),
    dsl::lit<"_cm">([](long double v){ return v / 100.0L; })
);
```

The variable `lits` has type `LiteralSet<LiteralEntry<...,lambda1>,
LiteralEntry<...,lambda2>, LiteralEntry<...,lambda3>>`, where each lambda type
is a distinct compiler-generated closure type. Because the closure types are
unique to the translation unit, the set type is also unique and cannot be
named in a header; the `auto` placeholder is essential.

The factory is the only intended construction path for a `LiteralSet`. Direct
construction is possible but offers no advantage, since the entry types are
awkward to name. Once constructed, the set is used either by calling its
`parse` member directly or by handing it to the `CustomLiterals` mixin as the
`Derived::literals` member, as described below.

## The parse Member Function

The `parse` member performs the actual classification. Its contract is to
treat the input string as a numeric prefix followed by a suffix drawn from the
table, to locate the first matching suffix, to convert the prefix with
`std::stold`, and to return the result of applying the matched handler to that
number. The return type is `long double` regardless of the handler's return
type; the handler's result is implicitly converted.

The matching condition is a suffix test. For an entry whose `suffix` has length
`n`, the entry matches when `input.size() > n` and the last `n` characters of
`input` equal `suffix`. The strict-greater comparison ensures that the numeric
prefix is non-empty; an input consisting only of the suffix, such as `_km`
with no leading digits, will not match an entry whose suffix is `_km`.

When a match is found, the prefix is extracted as `input.substr(0,
input.size() - suf.size())` and passed to `std::stold`. The handler is then
invoked with the resulting `long double`, and its return value is assigned to
`result`. The `found` flag is set, which short-circuits the remaining entries
through the early-return at the top of the fold-expression body.

The scan order is the declaration order of the entries in the set. The first
entry whose suffix matches the tail of the input wins; later entries are not
considered, even if they would also match. This first-match semantics is the
single most important behavioural detail of the feature, and it has
consequences for how overlapping spellings must be ordered, as discussed
below.

If no entry matches, `parse` throws `std::runtime_error` with a message of the
form `dsl::LiteralSet::parse: no suffix matched for '<input>'`. The function
does not return a sentinel or an optional; the failure mode is an exception.
DSL authors who need softer failure handling must catch the exception at the
call site, or pre-scan the input themselves before invoking `parse`.

## The CustomLiterals Feature Tag and Mixin

The `CustomLiterals` feature tag integrates the literal table with the CRTP
foundation described in Chapter 3. By listing `dsl::CustomLiterals` among the
feature tags of a `dsl::DSL` specialisation, the derived DSL acquires a
`parse_literal` static member that delegates to its `literals` member. The
feature is defined as:

```cpp
struct CustomLiterals
{
  template <typename Derived> struct Mixin
  {
    static long double
    parse_literal (std::string_view input)
      requires requires { Derived::literals; }
    {
      return Derived::literals.parse (input);
    }
  };
};
```

The tag is an empty struct whose only content is the nested `Mixin` template.
The mixin is injected into the CRTP base class chain by the `dsl::DSL`
template, so any DSL that lists `CustomLiterals` as a feature gains the
`parse_literal` member. The member is static because the literal table is
itself static state of the derived class.

The `requires requires { Derived::literals; }` clause is a constraint, not
merely documentation. It prevents the mixin from instantiating `parse_literal`
for derived classes that have not declared a `literals` member. Without that
member, the expression `Derived::literals.parse(input)` would be ill-formed,
and the constraint makes the removal clean rather than producing a cascade of
template errors. The constraint is re-expressed at the concept level by
`HasLiterals`, described next.

The derived class is responsible for declaring the `literals` member with the
correct type and value. The mixin does not synthesise it; it merely references
it. This separation keeps the feature tag stateless and lets the derived class
control the exact set of registered spellings, including their order.

## The parse_literal Entry Point

With the feature in place, parsing a literal string from the DSL becomes a
single static call. The derived class declares its table and inherits
`parse_literal`, which forwards to the table's `parse` member. The canonical
form, taken from the header documentation, is:

```cpp
struct UnitsDSL : dsl::DSL<UnitsDSL, dsl::CustomLiterals> {
    static constexpr auto literals = dsl::literal_set(
        dsl::lit<"_km">([](long double v){ return v * 1000.0L; }),
        dsl::lit<"_m"> ([](long double v){ return v; })
    );
};
auto val = UnitsDSL::parse_literal("5.0_km");  // 5000.0
```

The call `UnitsDSL::parse_literal("5.0_km")` resolves to the mixin's static
member, which invokes `UnitsDSL::literals.parse("5.0_km")`. The set scans its
entries, finds that `_km` matches the tail, extracts `5.0`, converts it to
`long double`, applies the kilometre handler, and returns `5000.0L`.

The return type is `long double` in all cases. A DSL whose handlers return
`int` or `double` will have those values converted to `long double` before
return. Callers that need the original type must bypass `parse_literal` and
call `literals.parse` directly, bearing in mind that `parse` also returns
`long double`.

Because `parse_literal` is static, no instance of the DSL is required. This
reflects the fact that the literal table is class-level configuration, not
instance state. It also means that two instances of the same DSL share the
same table and produce the same classification for the same input.

## The HasLiterals Concept

The `HasLiterals` concept identifies types that participate in the
`CustomLiterals` feature correctly. It is defined alongside the other feature
concepts described in Chapter 5:

```cpp
template <typename T>
concept HasLiterals
    = HasFeature<T, CustomLiterals> && requires { T::literals; };
```

The concept has two clauses. The first, `HasFeature<T, CustomLiterals>`,
checks that `T` lists `CustomLiterals` among its feature tags, which is the
structural requirement that the mixin be present in the base chain. The
second, `requires { T::literals; }`, checks that `T` actually declares a
member named `literals` that can be named in an unevaluated context.

Both clauses are necessary. A type could list the feature tag but forget to
declare `literals`, in which case the mixin's `parse_literal` would be
constrained away and the type would not actually support parsing. Conversely,
a type could declare a `literals` member without listing the feature tag, in
which case the mixin would not be present and `parse_literal` would not exist
even though the table does. `HasLiterals` requires both halves of the
contract.

The concept is the recommended way to constrain templates that accept a DSL
with literal support. A parser combinator that wants to consult a DSL's
literal table, for example, can require `HasLiterals<D>` rather than
re-implementing the two clauses inline. Generic code that calls
`parse_literal` should do so behind a `requires HasLiterals<T>` guard so that
overload resolution removes the candidate cleanly for non-literal DSLs.

## Matching Semantics: Suffix Match, First-Match Ordering

The behaviour of `parse` is best understood through its two governing rules:
the match is a suffix match, and the scan is a first-match scan. A suffix
match means that an entry matches the input only when the input ends with the
entry's spelling and the input is strictly longer than the spelling. A
first-match scan means that the entries are tried in declaration order and the
first match wins.

The suffix rule is what suits the feature to unit-decorated numerics. The
input `3.5_km` ends with `_km`, so the `_km` entry matches and the prefix
`3.5` is parsed. The input `3.5` alone does not end with any registered
suffix, so it does not match and `parse` throws. The input `_km` alone,
although it ends with `_km`, fails the strict-greater length test and also
does not match, because the numeric prefix would be empty and `std::stold`
would have nothing to consume.

The first-match rule has consequences when one suffix is a tail of another.
Suppose a set registers `_m` before `_km`. The input `3.5_km` ends with `_km`
but does not end with `_m`, because the last two characters are `km`, not
`_m`. The order is irrelevant in that particular case. Now suppose a set
registers `m` and `5m`, and the input is `3.5m`. The `m` entry matches
because the input ends with `m`. The `5m` entry, declared later, is never
reached, even though it also matches. The handler for `m` wins.

This is the central ordering pitfall of the feature: when one suffix is a
proper tail of another and both can match the same input, the entry declared
first shadows the entry declared second. The implementation does not perform
longest-match selection. A user who needs `5m` to win over `m` for the input
`3.5m` must declare `5m` before `m` in the `literal_set` argument list.

The toolkit does not check for shadowing at compile time. Two entries may
have identical spellings, in which case the second is simply unreachable.
Two entries whose spellings overlap only in suffix-vs-tail fashion are
permitted, and the order of declaration silently determines which one is
consulted first. Disciplined tables keep overlapping spellings ordered from
most specific to least specific.

## Registering Keywords

Although the feature is suffix-oriented, it adapts naturally to keyword
tables. A keyword is a token whose entire spelling matches a registered
literal. Because the suffix match degenerates to a whole-string match when
the prefix is required to be numeric, the keyword use case needs a handler
that ignores its numeric argument and yields a tag.

The trick is to provide a numeric prefix in the input. A lexer that has
already split the input into tokens can present each token together with a
leading numeric sentinel, or it can use a dedicated wrapper that calls the
handler with a dummy value. The more direct approach for keyword
classification is to bypass `parse` and iterate the entries directly, since
the suffix machinery is not the natural fit for whole-token matching.

When the suffix machinery is acceptable, a keyword table can be built as
follows. The handler returns an enumeration tag identifying the keyword.

```cpp
enum class Kw { If, Then, Else, Unknown };

constexpr auto keywords = dsl::literal_set(
    dsl::lit<"if">([](long double){ return Kw::If; }),
    dsl::lit<"then">([](long double){ return Kw::Then; }),
    dsl::lit<"else">([](long double){ return Kw::Else; })
);
```

Because the suffix match requires a strictly longer input, a bare token `if`
will not match the entry `"if"`; the input must be longer than the spelling.
Keyword classification therefore typically uses a small adapter that compares
the whole token against each entry's `suffix` directly, rather than calling
`parse`. The `LiteralSet` exposes its `entries` tuple publicly, so such an
adapter can iterate the entries with `std::apply` and compare spellings
exactly.

This adaptation shows the boundary between the `CustomLiterals` feature and
a full lexer. For unit-suffixed numerics the feature is a perfect fit; for
whole-token keyword classification the feature provides the table and the
spelling storage, while the caller provides the comparison strategy.

## Registering Operators

Operator tables raise the same considerations as keyword tables. An operator
such as `<=` is a short fixed spelling, and a DSL that classifies operator
tokens wants to map each spelling to a tag. The suffix machinery of `parse`
is not the natural dispatch for an operator token, because the token contains
no numeric prefix, but the `LiteralEntry` record is still a convenient way to
pair a compile-time spelling with a tag.

A typical operator table stores the spelling and yields an operator tag from
the handler, ignoring the numeric argument exactly as the keyword table does.

```cpp
enum class Op { Plus, Minus, Le, Ge };

constexpr auto operators = dsl::literal_set(
    dsl::lit<"+">([](long double){ return Op::Plus; }),
    dsl::lit<"-">([](long double){ return Op::Minus; }),
    dsl::lit<"<=">([](long double){ return Op::Le; }),
    dsl::lit<">=">([](long double){ return Op::Ge; })
);
```

Ordering matters for operators just as it does for unit suffixes. The spelling
`<` is a proper tail of `<=`. If `<` were registered and the dispatch were a
suffix match, an input ending in `<=` would not match `<` anyway because the
last character is `=`. For whole-token dispatch, however, a token `<` and a
token `<=` are distinct strings and there is no shadowing. The lesson is that
shadowing is a property of the matching rule, not of the table alone.

For longest-match token classification across a set of operators and
keywords, the parser combinator framework of Chapter 23 is the appropriate
tool. The `CustomLiterals` table is best regarded as a static spelling
registry that the combinator layer can consult, rather than as a complete
lexer.

## A Unit-Suffix Numeric Literal Set

The canonical application of the feature is a unit-conversion DSL. The
following DSL, drawn from the header examples, registers three length units
and exposes `parse_literal` through the mixin.

```cpp
struct UnitsDSL : dsl::DSL<UnitsDSL, dsl::CustomLiterals> {
    static constexpr auto literals = dsl::literal_set(
        dsl::lit<"_km">([](long double v){ return v * 1000.0L; }),
        dsl::lit<"_m"> ([](long double v){ return v; }),
        dsl::lit<"_cm">([](long double v){ return v / 100.0L; })
    );
};
```

Calling `UnitsDSL::parse_literal("3.5_km")` scans the table. The `_km` entry
matches the tail of `3.5_km`, the prefix `3.5` is converted to `long double`,
and the handler multiplies by one thousand, returning `3500.0L`. The `_m` and
`_cm` entries are not consulted because the first match short-circuits the
scan.

The same DSL handles `100_cm` by matching `_cm`, converting `100`, and
dividing by one hundred, yielding `1.0L`. An input such as `3.5mi` matches no
entry, and `parse_literal` throws `std::runtime_error` with the message
`dsl::LiteralSet::parse: no suffix matched for '3.5mi'`.

Because the table is `static constexpr`, it is constructed once and shared
across all calls. There is no per-call allocation, no hash-table lookup, and
no virtual dispatch. The fold-expression over the entry tuple is expanded by
the compiler into a straight-line sequence of comparisons, which optimisers
typically reduce further.

The ordering of the three entries above is safe because no suffix is a tail of
another: `_km` ends with `m` but not with `_m`, `_cm` ends with `m` but not
with `_m`, and `_m` is distinct from both. A table that added a bare `m`
suffix would have to place `m` after `_km` and `_cm` to avoid shadowing them
on inputs such as `3.5_km`, although as noted `_km` does not end with `_m` and
so the bare `_m` would not in fact capture it; the shadowing concern arises
only for inputs that genuinely end with the shorter spelling.

## A Tiny Token Classifier

To illustrate the feature as a building block inside a larger front end,
consider a small token classifier that uses a `LiteralSet` to map operator
spellings to tags. The classifier iterates the entries directly rather than
calling `parse`, because tokens carry no numeric prefix. The `entries` member
is public, so the iteration is straightforward.

```cpp
#include "DSLtk.hpp"
#include <string_view>
#include <tuple>
#include <iostream>

enum class Tok { Plus, Minus, Le, Ge, Unknown };

constexpr auto ops = dsl::literal_set(
    dsl::lit<"+">([](long double){ return Tok::Plus; }),
    dsl::lit<"-">([](long double){ return Tok::Minus; }),
    dsl::lit<"<=">([](long double){ return Tok::Le; }),
    dsl::lit<">=">([](long double){ return Tok::Ge; })
);

Tok classify(std::string_view tok) {
    Tok result = Tok::Unknown;
    std::apply(
        [&](const auto&... e) {
            (([&] {
                if (tok == e.suffix) result = e.handler(0.0L);
            }()), ...);
        },
        ops.entries);
    return result;
}
```

The `classify` function walks the entry tuple with `std::apply` and a
fold-expression, comparing each token against the entry's `suffix` view. The
first exact match assigns the tag by invoking the handler with a dummy
`0.0L`. Because the comparison is exact, there is no shadowing between `<`
and `<=`: a token `<` matches only the `<` entry, and a token `<=` matches
only the `<=` entry.

This pattern shows how the `LiteralSet` doubles as a generic spelling table.
The `parse` member is the convenience path for suffix-decorated numerics, but
the underlying entry tuple is available for any dispatch strategy the caller
wishes to implement. The compile-time spellings, the typed handlers, and the
ordered tuple are useful independently of the suffix convention.

For a production lexer, the parser combinators of Chapter 23 and the
primitive parsers of Chapter 24 offer a more complete solution, including
position tracking, diagnostics, and composition. The token classifier above
is intended only to show how `LiteralSet` participates in a hand-written
front end.

## Combining with the Pipeline Feature

Example 18 of the toolkit combines `CustomLiterals` with the `Pipeline`
feature described in Chapter 6. The DSL registers unit suffixes through
`CustomLiterals` and uses the `Pipeline` mixin's `wrap` to feed the parsed
value into a chain of transformations. The example is reproduced below.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct MiniDSL : dsl::DSL<MiniDSL, dsl::Pipeline, dsl::CustomLiterals> {
  static constexpr auto literals = dsl::literal_set(
      dsl::lit<"_km">([](long double v) { return v * 1000.0L; }),
      dsl::lit<"_m">([](long double v) { return v; }));
};

int main() {
  auto meters = MiniDSL::parse_literal("2.5_km");
  auto shown = MiniDSL::wrap(meters) | dsl::pipe([](long double x) { return x + 10.0L; });
  std::cout << shown << "\n";
}
```

The two feature tags are listed together in the `dsl::DSL` argument list.
The CRTP base class composes the two mixins, so `MiniDSL` gains both
`parse_literal` from `CustomLiterals` and `wrap` from `Pipeline`. The call
`MiniDSL::parse_literal("2.5_km")` returns `2500.0L`, which is then wrapped
and piped through a lambda that adds ten, yielding `2510.0L`.

This composition is the intended use of the feature architecture. Each
feature contributes one capability, and the DSL author combines the
capabilities by listing the corresponding tags. The `CustomLiterals` feature
contributes the table and the `parse_literal` entry point; the `Pipeline`
feature contributes the `wrap` adapter and the `pipe` operator; together they
turn a string literal into a transformed numeric value in a single
expression.

The combination also illustrates the role of `long double` as the lingua
franca of the feature. The `Pipeline` in this example carries `long double`
values, and `parse_literal` returns `long double`, so the two features
interoperate without any explicit adapter. A DSL whose handlers returned a
different type would need a conversion at the boundary, because
`parse_literal` normalises to `long double` regardless.

## Registering Constants

Beyond units and keywords, the feature can register named constants. A
constant is a literal whose handler ignores its numeric argument and returns a
fixed value. The spelling serves as the name, and the table maps the name to
its value. Because `parse` requires a numeric prefix, the constant use case
again benefits from direct entry iteration rather than the `parse` member.

A constants table can be built with `lit<>` using handlers that capture the
constant value by copy.

```cpp
constexpr auto constants = dsl::literal_set(
    dsl::lit<"pi">([](long double){ return 3.141592653589793238462643L; }),
    dsl::lit<"e">([](long double){ return 2.718281828459045235360287L; }),
    dsl::lit<"tau">([](long double){ return 6.283185307179586476925286L; })
);
```

A caller that iterates `constants.entries` and compares spellings exactly can
resolve the name `pi` to its value. The handler's `long double` parameter is
unused and is present only to satisfy the `F(long double) -> T` signature
expected by `LiteralEntry`. This is a minor awkwardness of the unified handler
shape, which is tuned for the numeric-suffix case.

For numeric-suffixed constants, such as a hypothetical `1k` meaning one
thousand, the `parse` member is again the natural path: the input `1k` ends
with `k`, the prefix `1` is converted, and the handler multiplies by one
thousand. The same table can therefore mix named constants and scaled
numerics, provided the spellings are chosen so that the suffix matches
disambiguate correctly.

## Design Notes: Why the Suffix Convention

The suffix convention may seem restrictive for a feature named "custom
literals" in general. The restriction is a deliberate alignment with the
dominant use case: unit-decorated numerics, where the lexical structure is a
number followed by a unit marker. By fixing the matching rule to a suffix
test, the feature can offer a single `parse` member that handles this case
without any configuration, and the handler signature can be fixed to
`F(long double) -> T` so that the table type is uniform.

The cost of this design is that whole-token classification, as for keywords
and operators, is not directly served by `parse`. The toolkit addresses that
cost by exposing the `entries` tuple, so a caller can implement any matching
rule against the same compile-time spellings. The `LiteralSet` is thus both a
parser for the numeric-suffix case and a generic spelling table for other
cases.

The first-match ordering is a similar trade-off. A longest-match rule would
remove the shadowing pitfall but would require the implementation to scan
all entries and select the longest match, which is more work per call and
which complicates the handler dispatch. First-match is simpler, is
predictable, and lets the author express an explicit priority by ordering the
entries. The author who wants longest-match can achieve it by ordering
entries from longest spelling to shortest, so that the first match is also
the longest.

## The Lifecycle of a Literal Set

A `LiteralSet` is a value type with no ownership beyond its tuple of
callables. Lambdas with empty captures are stateless and occupy no storage,
so a set composed of captureless lambdas is effectively empty at run time,
even though its type encodes a rich table. Sets that capture state, such as
a lookup table captured by copy, do carry that state in the tuple.

The intended lifetime of a set is static. A `static constexpr` member of a
DSL class is constructed during constant initialisation and lives for the
duration of the program. A set can also be a local `constexpr` variable, in
which case it lives for the enclosing scope. Passing a set by const reference
to a function is cheap, because the type is small and the spellings are
static.

Sets are not mutated after construction. The `parse` member is `const`, and
there is no public interface for adding or removing entries. A DSL that needs
a different table at run time must select between pre-built sets, rather than
mutating one set. This immutability is consistent with the compile-time
nature of the spellings: since the spellings are template arguments, the type
of the set cannot change at run time, and so the set itself is treated as
immutable.

The exception thrown on failure is the only observable side effect of
`parse`. The function performs no allocation, no I/O, and no global-state
mutation. The `std::stold` call may throw `std::invalid_argument` or
`std::out_of_range` if the numeric prefix is malformed, even when the suffix
matches; this exception propagates through `parse` to the caller. A robust
caller should be prepared to catch either the runtime error from a missing
suffix or the conversion exceptions from a malformed prefix.

## A Complete Units Example

The following complete program defines a units DSL, parses several suffixed
numerics, and prints the results. It is a superset of the header example and
is written to compile with `-std=c++20`.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct UnitsDSL : dsl::DSL<UnitsDSL, dsl::CustomLiterals> {
    static constexpr auto literals = dsl::literal_set(
        dsl::lit<"_km">([](long double v){ return v * 1000.0L; }),
        dsl::lit<"_m"> ([](long double v){ return v; }),
        dsl::lit<"_cm">([](long double v){ return v / 100.0L; }),
        dsl::lit<"_mm">([](long double v){ return v / 1000.0L; })
    );
};

int main() {
    std::cout << UnitsDSL::parse_literal("3.5_km") << "\n";  // 3500
    std::cout << UnitsDSL::parse_literal("100_cm") << "\n";  // 1
    std::cout << UnitsDSL::parse_literal("750_mm") << "\n";  // 0.75
    try {
        UnitsDSL::parse_literal("3.5mi");
    } catch (const std::runtime_error& e) {
        std::cout << "caught: " << e.what() << "\n";
    }
}
```

The four entries are ordered safely: `_km`, `_m`, `_cm`, `_mm` are pairwise
distinct as suffixes, and none is a tail of another that would cause
shadowing on the test inputs. The output is `3500`, `1`, `0.75`, and the
diagnostic message for the unmatched `3.5mi` input.

The program also demonstrates the failure path. The input `3.5mi` matches
none of the four suffixes, so `parse` throws and the `catch` block reports
the message. A DSL that wanted to treat unmatched input as zero, rather than
as an error, would wrap the call in a helper that catches the exception and
returns a default.

## The HasLiterals Concept in Use

Generic code that accepts a DSL with literal support should constrain its
template parameter with `HasLiterals`. The following function template
parses a value from any literal-supporting DSL and applies a scaling factor,
returning zero on failure.

```cpp
template <typename D>
requires dsl::HasLiterals<D>
long double scaled_value(std::string_view s, long double factor) {
    try {
        return D::parse_literal(s) * factor;
    } catch (const std::runtime_error&) {
        return 0.0L;
    }
}
```

The constraint ensures that `D::parse_literal` exists and is callable. Without
the constraint, the call would be ill-formed for a DSL that lacks the
`CustomLiterals` feature, and the error would surface deep inside the
template instantiation. With the constraint, overload resolution removes the
candidate for non-literal DSLs, and the diagnostic is local to the call site.

The concept also serves as documentation. A reader who sees
`requires dsl::HasLiterals<D>` knows immediately that the function depends on
the literal table, without inspecting the body. This is the same role that
the other feature concepts of Chapter 5 play for their respective features.

A function that needs to combine literal support with another feature can
conjoin the concepts. For example, a function that requires both literal
parsing and pipeline wrapping would write
`requires dsl::HasLiterals<D> && dsl::HasPipeline<D>`, assuming a
corresponding `HasPipeline` concept. The feature concepts are designed to
compose in this way, reflecting the orthogonality of the features themselves.

## Pitfalls and Common Mistakes

The first pitfall is misspelling the suffix. Because the spelling is a
`FixedString` non-type template argument, a typo such as `"_Kn"` instead of
`"_km"` compiles cleanly but never matches the intended inputs. The table
silently fails to recognise the unit, and `parse` throws at run time. A
table should be tested against representative inputs to catch such typos
early.

The second pitfall is the strict-greater length test. An input that consists
of the suffix alone, with no numeric prefix, does not match. Calling
`parse_literal("_km")` throws, because the input length equals the suffix
length and the strict-greater test fails. Callers that present whole tokens
to `parse` must ensure that the token carries a numeric prefix, or they must
use the direct-entry-iteration pattern shown earlier.

The third pitfall is shadowing between overlapping suffixes. When one suffix
is a proper tail of another and both can match the same input, the entry
declared first wins. The implementation does not perform longest-match
selection. Tables that mix overlapping spellings must order them from most
specific to least specific to avoid unintended shadowing. The ordering is a
property of the table that the author must maintain.

The fourth pitfall is handler return-type conversion. The `parse` member
returns `long double` regardless of the handler's return type. A handler
that returns `int` will have its result widened, with possible loss of
precision for very large values. A handler that returns a non-numeric type
will fail to compile, because the assignment to `result` requires a
conversion to `long double`. Handlers should be written with this conversion
in mind.

The fifth pitfall is the exception on no match. Code that calls `parse` or
`parse_literal` must be prepared to catch `std::runtime_error`, or it must
pre-validate the input. A loop that parses many inputs and lets the exception
propagate will abort on the first unrecognised input, which is rarely the
desired behaviour for a batch lexer.

The sixth pitfall is assuming that the feature is a general lexer. It is not.
The `CustomLiterals` feature is a static, suffix-oriented spelling table with
a first-match dispatch. For full lexer concerns such as position tracking,
character classes, longest-match across alternatives, and diagnostics, the
parser combinator framework of Chapter 23 and the PEG machinery of Chapter 27
are the appropriate tools. The literal table can feed into those frameworks,
but it does not replace them.

## Interoperability with Native User-Defined Literals

The header documentation notes that the feature does not itself define native
C++ user-defined literal operators such as `operator""_km`. Native UDL syntax,
writing `3.5_km` directly in C++ source, requires an `operator""_km` defined
in an inline namespace, which is a language feature separate from the
`CustomLiterals` table. The header suggests defining such operators in the
user's own header when native syntax is desired.

The two mechanisms are complementary. The `CustomLiterals` table parses
strings at run time, while native UDL operators parse literals at compile
time in C++ source. A DSL that wants both can define its table with
`literal_set` for run-time string parsing and can additionally provide
`operator""` overloads that invoke the same conversion logic for compile-time
use. The following sketch shows the pattern.

```cpp
inline namespace unit_literals {
    constexpr long double operator""_km(long double v) { return v * 1000.0L; }
    constexpr long double operator""_m (long double v) { return v; }
}
// Usage:
// using namespace unit_literals;
// auto d = 3.5_km;  // 3500.0
```

The native operators are entirely the user's responsibility. The toolkit
provides the table and the `parse_literal` entry point; it does not generate
the operators. This separation keeps the feature focused on run-time string
classification and leaves the compile-time UDL machinery, which has its own
language rules, to the user.

## Summary

The `CustomLiterals` feature provides a compile-time registry of named string
spellings and a run-time classifier that consults it. The registry is built
from `LiteralEntry` records, each pairing a `FixedString` spelling with a
handler callable. The `lit<>` factory deduces the handler type so that only
the spelling must be written explicitly. The `literal_set` variadic factory
aggregates entries into a `LiteralSet`, which stores them as a typed tuple
and exposes a `parse` member.

The `parse` member treats its input as a numeric prefix followed by a suffix
drawn from the table. It scans the entries in declaration order and applies
the handler of the first entry whose suffix matches the tail of the input.
The first-match rule means that overlapping suffixes must be ordered
carefully: the entry declared first shadows later entries that would also
match. There is no longest-match selection; authors who need longest-match
must enforce it by ordering entries from most specific to least specific.
On no match, `parse` throws `std::runtime_error`.

The `CustomLiterals` feature tag integrates the table with the CRTP
foundation. A DSL that lists the tag and declares a `static constexpr auto
literals` member gains a `parse_literal` static entry point that delegates to
the table. The `HasLiterals` concept constrains types that satisfy this
contract and is the recommended way to guard generic code that calls
`parse_literal`.

The feature composes orthogonally with other features. Combined with the
`Pipeline` feature of Chapter 6, it turns a string literal into a transformed
numeric value in a single expression. Combined with the parser combinators of
Chapter 23 and the primitive parsers of Chapter 24, it provides a static
spelling table that a larger front end can consult. For whole-token
classification, such as keyword and operator tables, the `entries` tuple is
available for direct iteration, since the `parse` member's suffix convention
is tuned for numeric-decorated inputs rather than for bare tokens.

The feature is narrow by design. It is not a general lexer, it does not
perform longest-match selection, and it does not generate native C++ UDL
operators. Within its scope, it offers a typed, ordered, compile-time
spelling table with a predictable first-match dispatch, suitable for
unit-suffixed numerics and as a building block for larger front ends.
