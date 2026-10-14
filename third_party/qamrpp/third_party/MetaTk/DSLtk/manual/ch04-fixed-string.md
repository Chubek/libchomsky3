# Chapter 4: `FixedString`: Compile-Time Strings as Template Parameters

DSLtk is built around a single recurring idea: the names, tags, patterns, and rule identifiers that shape a domain-specific language should be known at compile time. A grammar rule called `"add"`, a token tag called `"num"`, and a regular-expression pattern like `"[0-9]+"` are not runtime configuration values but structural facts about the program. Carrying those facts through the type system lets the compiler optimize, check, and dispatch on them without any runtime cost.

The obstacle, of course, is that C++ has historically refused to accept a string literal as a non-type template parameter (NTTP). This chapter introduces `dsl::FixedString`, the small literal class type that removes that obstacle. Every downstream chapter that writes something inside angle brackets — `dsl::leaf<"num">`, `dsl::node<"add">`, `dsl::pattern<"[0-9]+">`, `dsl::lit<"_km">`, `dsl::rule<"name">`, `dsl::when<...>`, and the PEG `add_rule<"...">` — depends on the type defined here.

## The Problem: Strings as Template Parameters

Before C++20, the non-type template parameter space was narrow. A template could be parameterized by integral constants, enumerators, pointers, and references to objects with static storage duration. A string literal, however, is an array of `const char` with automatic storage duration, and arrays decay to pointers in most contexts. The obvious attempt fails to compile.

```cpp
template <const char *Tag> struct leaf;   // declaration

leaf<"num"> n;                             // error before C++20
```

The compiler rejects this because `"num"` does not designate an object with static storage duration in the sense the standard requires for a pointer NTTP. Even when a workaround using a reference to an array was employed, each distinct tag still required a separate named variable declared at namespace scope, which defeated the goal of writing self-documenting inline tags.

The consequence for a library like DSLtk would be severe. Instead of writing `dsl::node<"add">(lhs, rhs)`, a user would have to declare `static constexpr char add_tag[] = "add";` somewhere and then write `dsl::node<add_tag>(lhs, rhs)`. The tag would no longer live at the point of use, grammar definitions would sprawl across declarations, and the pedagogical clarity that DSLtk is designed to deliver would evaporate. A type-level solution is needed.

The same obstacle blocks more than convenience. Many of DSLtk's compile-time mechanisms — pattern dispatch in `when<>`, rule naming in the rewrite system, production naming in the PEG layer — rely on the compiler being able to distinguish one string from another at instantiation time. Without a string NTTP, each of those mechanisms would have to fall back on runtime comparison of `std::string` values, which forfeits inlining, type-level dispatch, and the guarantee that two grammatically identical tags denote the same specialization. The library's design depends on the tag being part of the type, not part of the data.

## C++20 Class-Type Non-Type Template Parameters

C++20 relaxed the NTTP rules in a targeted way. A literal class type may now be used as a non-type template parameter provided that all of its non-static data members are of structural types, none are of reference type, and the class meets the usual literal-type requirements. Structural types include scalar types, lvalue reference types, and arrays of structural types. A class containing a plain `char` array therefore qualifies.

This is the door that `FixedString` walks through. Because it is a literal class type whose only non-static data member is a `char` array, it is a valid NTTP. Each distinct string literal produces a distinct `FixedString<N>` value, and the compiler treats two `FixedString` values with the same contents and length as the same template argument. The library can finally write `leaf<"num">` and mean it.

The structural-type rule has a subtle but important consequence. Two template arguments of class type are considered to designate the same template parameter when their corresponding members have the same values. For `FixedString`, that means the bytes of `data` must match element by element. This is exactly the semantics a tag type should have: two nodes tagged `"num"` are interchangeable regardless of where they were constructed.

## The `FixedString<N>` Definition

The type is deliberately minimal. The entire definition fits in ten lines and depends only on `<cstddef>` for `std::size_t`, `<algorithm>` for `std::copy_n`, and `<string_view>` for the `view()` return type. Quoted verbatim from `DSLtk.hpp`:

```cpp
template <std::size_t N> struct FixedString
{
  char data[N]{};
  constexpr FixedString (const char (&s)[N]) { std::copy_n (s, N, data); }
  constexpr std::string_view
  view () const
  {
    return { data, N - 1 };
  }
  constexpr bool operator== (const FixedString &) const = default;
};
```

Every member is `constexpr`, every operation is trivially copyable, and the type has no virtual functions, no base classes, and no reference members. The design is intentionally compatible with being stored in the compiler's internal representation of a template argument.

The single template parameter `N` is the length of the string including its null terminator. A `FixedString<4>` therefore holds three visible characters plus a trailing `'\0'`. This convention matches the declared type of a string literal in C++, which is `const char[N+1]` for an N-character literal, and it is what allows the constructor to deduce `N` directly from a literal argument.

The empty string is a valid special case. A `FixedString<1>` carries only the terminator and represents a zero-length view. It arises naturally in the library whenever a rule or suffix is intentionally left blank, and the structural-equality rules treat all such instances as the same specialization. No special-casing is required in the definition to support it.

## Storage and the Null Terminator

The data member `char data[N]{}` is value-initialized, so every byte starts at zero. When the constructor runs, it copies all `N` bytes from the source array, terminator included, into `data`. The trailing null is therefore always present, which makes `data` safe to pass to any C API that expects a NUL-terminated string in addition to being usable through `view()`.

The null terminator is not merely decorative. Because `view()` returns a `std::string_view` of length `N - 1`, the visible string never includes the terminator; but the raw `data` array still ends in `'\0'`, so `data` can be used directly where a C-style string is required. This dual nature — a C string when you need one, a sized view when you do not — is a small but persistent convenience throughout the library.

The fact that `N` is part of the type is the central design trade-off. A `FixedString<4>` and a `FixedString<5>` are unrelated types as far as the type system is concerned, even if both happen to spell `"num"` when padded. In practice this is not a limitation, because every literal in a program has a single, fixed length known to the compiler, and the deduction in the constructor picks the correct `N` automatically.

## The Constructor and Array-Reference Deduction

The constructor's parameter type, `const char (&s)[N]`, is a reference to an array of exactly `N` `const char`. This is the idiom that makes string-literal deduction work. When the constructor is called with `"abc"`, the literal has type `const char[4]`, so `N` is deduced as 4 and the constructor receives a reference to that exact array. No decay to `const char*` occurs, and no length information is lost.

```cpp
constexpr dsl::FixedString a = "abc";   // N deduced as 4
static_assert(a.view() == "abc");
static_assert(a.view().size() == 3);
```

The body copies all `N` bytes with `std::copy_n(s, N, data)`. Because `N` includes the terminator, the copy carries the trailing `'\0'` into `data` as well. `std::copy_n` is itself `constexpr` in C++20, so the whole constructor is usable during constant evaluation.

The use of `std::copy_n`, rather than a hand-rolled loop, is a deliberate readability choice. The standard algorithm is recognized by optimizers and produces the same code as a `memcpy` on trivially copyable types, while remaining explicitly `constexpr`. The constructor body therefore stays a single statement, which keeps the definition short enough to inspect at a glance.

Because the parameter is a reference to an array of fixed size, the constructor rejects arguments whose size does not match the deduced `N`. This is not a runtime check; it is a compile-time consequence of the parameter type. Passing a `std::string` or a `const char*` simply does not match the constructor signature, which prevents accidental loss of length information.```cpp
dsl::FixedString b("hello");            // N deduced as 6
// dsl::FixedString c(std::string("x")); // error: no matching constructor
```

The constructor is not `explicit`, which allows the implicit conversion that takes place when a string literal is supplied as a template argument. Writing `dsl::leaf<"num">(42)` relies on the compiler implicitly constructing a `FixedString` from `"num"` at the template-argument position. This implicit conversion is the ergonomic keystone of the whole library.

## The `view()` Method

The `view()` member returns a `std::string_view` spanning the first `N - 1` characters of `data`. The length is one less than the array size precisely because the null terminator is excluded from the logical string. The returned view is a lightweight handle — a pointer and a length — and incurs no allocation.

```cpp
constexpr dsl::FixedString tag = "identifier";
constexpr std::string_view v = tag.view();
static_assert(v.size() == 10);
static_assert(v == "identifier");
```

The method is declared `constexpr` so that it can be evaluated during constant expression. This matters because the library uses `view()` inside `static constexpr` initializers, inside `static_assert`, and inside the bodies of `constexpr` functions such as `leaf` and `node`. Without `constexpr`, the tag could not propagate into the type system at compile time.

The `std::string_view` return type was chosen over `std::string` deliberately. Returning a `std::string` would require dynamic allocation, which is not permitted during constant evaluation and would defeat the zero-overhead goal. A `string_view`, by contrast, is trivially copyable and points directly into the `data` array stored inside the `FixedString` object, which lives as long as the template argument that holds it.

A consequence of this design is that the `string_view` returned by `view()` is only valid for as long as the `FixedString` from which it came. In DSLtk this is never a problem, because the `FixedString` is a template argument and therefore lives for the duration of the program. Users who copy a `view()` result into a local `string_view` variable should keep this lifetime in mind, but in ordinary usage the view is consumed immediately.

It is worth emphasizing that `view()` never allocates and never throws. The `std::string_view` constructor it invokes is trivial, and `data` is always a valid, NUL-terminated array. This makes `view()` safe to call from `noexcept` contexts and from code paths that must remain allocation-free, such as the hot paths of the matcher and the AST dumper.

## The Defaulted `operator==`

The final member is a defaulted equality operator. C++20's defaulted comparisons perform memberwise comparison, so two `FixedString` objects compare equal if and only if every element of `data` matches. Because `N` is part of the type, comparisons only occur between objects of the same length, and the comparison reduces to a simple byte-by-byte test over the array.

```cpp
constexpr dsl::FixedString x = "add";
constexpr dsl::FixedString y = "add";
constexpr dsl::FixedString z = "sub";
static_assert(x == y);
static_assert(!(x == z));
```

The `default` keyword ensures that the operator is both `constexpr` and usable during constant evaluation. It also makes the comparison strong in the C++20 sense: there is no need to write a custom body, and the compiler is free to optimize the array comparison as it sees fit. The structural-equality semantics required for NTTPs align exactly with the semantics of this defaulted operator, so two `FixedString` template arguments that compare equal also designate the same template specialization.

Because equality is `constexpr`, tags can be compared at compile time inside `if constexpr`, inside `static_assert`, and inside any `consteval` context. This is what enables the `when<>` dispatch machinery described in Chapter 8 to select among branches at compile time: each branch's key is a `FixedString` (possibly wrapped in a `pattern`), and the selection is a constant-expression comparison.

## All-`constexpr` Design

Every operation `FixedString` provides — construction, `view()`, and equality — is `constexpr`. The type is therefore usable in any constant-expression context, including the initialization of `static constexpr` data members, the arguments of `static_assert`, the bodies of `consteval` functions, and, crucially, the evaluation of template arguments. This uniformity is not an accident; it is the property that makes the type usable as an NTTP with runtime-accessible values.

```cpp
template <dsl::FixedString S> struct tag_of
{
  static constexpr std::string_view value = S.view ();
};

using add_tag = tag_of<"add">;
static_assert(add_tag::value == "add");
```

The fact that the bytes of a `FixedString` template argument are readable at runtime, even though the argument itself is fixed at compile time, is what bridges the compile-time and runtime worlds. A function instantiated with `FixedString<"add">` can call `Tag.view()` at runtime and obtain a perfectly ordinary `std::string_view` that spells `"add"`. This is how `dsl::node<"add">` ends up labelling its `ASTNode` with a runtime string: the tag is a compile-time fact, but the label is read out at runtime through `view()`.

This bridge is one-directional. A compile-time `FixedString` can be projected into a runtime `string_view`, but a runtime `string_view` cannot be lifted back into a `FixedString` NTTP. DSLtk respects this asymmetry throughout: tags flow outward into runtime strings for display and matching, but never inward from runtime data into the type system. Recognizing this boundary is the key to using the library without fighting the language.

## How `FixedString` Flows Into the Library

`FixedString` is not a standalone utility; it is the connective tissue of the entire library. Almost every chapter that follows uses it in some form. The pattern is always the same: a string literal appears inside angle brackets, the compiler implicitly constructs a `FixedString` from it, and the resulting NTTP parameterizes a class or function template.

The most direct uses are `dsl::leaf` and `dsl::node`, which build the leaf and inner nodes of an AST. Their signatures, quoted from `DSLtk.hpp`, show the `FixedString Tag` parameter in the leading position.

```cpp
template <FixedString Tag, typename T>
ASTNode leaf (T &&val);

template <FixedString Tag, typename... Children>
  requires (std::convertible_to<Children, ASTNode> && ...)
ASTNode node (Children &&...children);
```

Inside `leaf`, the tag is read out at runtime with `Tag.view()` and stored as the `ASTNode`'s label. The value is streamed into a string and stored as the node's payload. The tag never becomes a runtime variable until `view()` is called; before that, it is purely a compile-time fact carried in the type.

```cpp
auto n = dsl::leaf<"token">("identifier");
// n.tag() == "token", n payload == "identifier"
```

The `pattern<S>` template, used by the compile-time regex engine described in Chapter 9, takes its regular-expression source as a `FixedString`. The signature is `template <FixedString S> struct pattern`, and the body stores a `static constexpr std::string_view value = S.view()`. The pattern string is therefore baked into the type at compile time and available to the matcher without any runtime construction.

```cpp
template <dsl::FixedString S> struct pattern
{
  static constexpr std::string_view value = S.view ();
  // ... matching machinery ...
};
```

The custom-literal machinery in Chapter 17 uses `FixedString` to name the suffix of a literal. The `lit` factory and its `LiteralEntry` carrier parameterize on `FixedString Suffix`.

```cpp
template <FixedString Suffix, typename F> struct LiteralEntry
{
  F handler;
  static constexpr std::string_view suffix = Suffix.view ();
  // ...
};

template <FixedString Suffix, typename F>
constexpr auto lit (F &&f);
```

A usage such as `dsl::lit<"_km">([](long double v){ return v * 1000.0L; })` binds the suffix `"_km"` into the type at compile time, so that the parser can recognise the suffix without any runtime lookup table.

The rewrite system in Chapter 13 names its rules with `FixedString`. A `RewriteRule<Tag, Pred, Trans>` carries the rule name as a compile-time string, allowing rewrite sets to be addressed and composed by name. The `dsl::rule<"name">` factory (Chapter 13 and Chapter 14) follows the same pattern.

```cpp
template <FixedString Tag, typename Pred, typename Trans>
struct RewriteRule;
```

The `when<>` dispatch clauses of Chapter 8 use `FixedString` indirectly through `pattern<"...">`, but they also accept integral and enumerator keys. When a `when` clause is keyed on a `pattern`, the underlying `FixedString` is what carries the regular-expression text into the type system.

```cpp
dsl::match(input,
  dsl::when<dsl::pattern<"[0-9]+">>([](auto){ return "number"; }),
  dsl::when<dsl::pattern<"[a-z]+">>([](auto){ return "word"; }),
  dsl::otherwise([](auto){ return "other"; })
);
```

Finally, the PEG grammar layer of Chapter 27 and Chapter 28 uses `FixedString` to name productions. The `add_rule<"...">` member and the `ProductionHandler<Id, Fn>` carrier both key on a `FixedString`, so an entire PEG grammar can be spelled out in compile-time strings without a single runtime identifier table.

## Building Tagged Nodes

The most common use of `FixedString` is labelling AST nodes. Chapter 11 covers `leaf` and `node` in depth; here we focus on the role of the tag. The tag is supplied as a string literal in angle brackets, and the compiler converts it to a `FixedString` before the template is instantiated.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main ()
{
  auto x = dsl::leaf<"x">(1);
  auto y = dsl::leaf<"y">(2);
  auto add = dsl::node<"add">(x, y);
  std::cout << add.tag() << " " << add.dump() << "\n";
  // prints: add (add (x 1) (y 2))
}
```

The tag `"add"` is a compile-time fact about the `node` call. It cannot be changed at runtime, it cannot be supplied from a config file, and it cannot be built from user input. This rigidity is intentional: the structure of the tree is part of the program's design, not part of its runtime data.

Because the tag is part of the type, two `node` calls with different tags instantiate different specializations of the `node` template. The compiler is free to inline each specialization independently, and there is no virtual dispatch or string-table lookup involved in constructing a node. The only runtime work is the `std::ostringstream` formatting of the value and the construction of the `ASTNode` itself.

The example program from `examples/02-fixed-strings.cpp` exercises this path end to end with only a handful of lines.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto n = dsl::leaf<"token">("identifier");
  auto t = dsl::node<"pair">(n, dsl::leaf<"kind">("name"));
  std::cout << t.tag() << " " << t.dump() << "\n";
}
```

Here `"token"` and `"kind"` are leaf tags, and `"pair"` is an inner-node tag. Each is a distinct `FixedString` NTTP, and each flows through to the `ASTNode`'s label via `view()`. The output is `pair (pair (token identifier) (kind name))`, demonstrating that the compile-time tags have become ordinary runtime strings by the time the tree is dumped.

## Comparing Tags

Because `operator==` is defaulted and `constexpr`, tags can be compared at compile time. This is useful whenever the structure of a tree is to be inspected by type rather than by value. A simple predicate over a node's tag can be written as a `constexpr` function.

```cpp
template <dsl::FixedString Tag>
constexpr bool is_add (std::string_view t)
{
  return t == Tag.view ();
}

static_assert(is_add<"add">("add"));
static_assert(!is_add<"add">("sub"));
```

Two tags of the same length compare equal only when every byte matches, which is exactly the structural-equality rule the language applies to class-type NTTPs. This means that `is_add<"add">` and a hypothetical second instantiation `is_add<"add">` are the same specialization, and the compiler will not emit duplicate code for them.

Runtime comparison works just as well. The defaulted `operator==` is callable on ordinary runtime objects, so a function that receives two `FixedString` values (for example, by storing them in a container) can compare them with the usual `==` syntax.

```cpp
bool same (dsl::FixedString<4> a, dsl::FixedString<4> b)
{
  return a == b;
}
```

Note that the two arguments must have the same `N`, because `FixedString<4>` and `FixedString<5>` are distinct types. When comparing tags of potentially different lengths, the correct approach is to compare their `view()` results, since `std::string_view::operator==` handles differing lengths correctly.

The distinction between structural equality on the type and string equality on the view matters in two places. Inside the type system, two `FixedString` arguments are the same template argument only when their bytes match exactly, so a `FixedString<"ab">` and a `FixedString<"ab\0">` (length three plus terminator) would be distinct specializations even though their visible text is the same. At runtime, by contrast, `view()` collapses such distinctions because it compares only the visible characters. Most library code compares views, which is the more forgiving of the two.

```cpp
template <auto A, auto B>
constexpr bool same_tag ()
{
  return A.view() == B.view();
}

static_assert(same_tag<dsl::FixedString{"abc"}, dsl::FixedString{"abc"}>());
static_assert(!same_tag<dsl::FixedString{"abc"}, dsl::FixedString{"abcd"}>());
```

## Tags as Dispatch Keys

The `when<>` machinery described in Chapter 8 uses `FixedString`-backed keys to build a compile-time dispatch table. Each `when` clause carries its key as a template argument, and the `match` implementation walks the clauses in order, comparing the input against each key. When the key is a `pattern<"...">`, the comparison is a regular-expression match; when the key is a `FixedString`-convertible value, the comparison is structural.

This pattern replaces the runtime `switch`-on-string idiom that C++ does not support natively. Instead of a `std::unordered_map<std::string, std::function>`, the dispatch table is encoded directly in the type system, and the compiler is free to inline each branch.

```cpp
auto label = dsl::match("add",
  dsl::when<dsl::pattern<"add">>([](auto){ return "plus"; }),
  dsl::when<dsl::pattern<"sub">>([](auto){ return "minus"; }),
  dsl::otherwise([](auto){ return "unknown"; })
);
```

The tags `"add"` and `"sub"` are `FixedString` values embedded in the `pattern` specializations. They never appear as runtime strings in the dispatch machinery itself; only their `view()` results do, and only at the point where the matcher actually compares them against the input.

## A Tagged-Union-of-Values Demo

A small but complete demonstration shows how `FixedString` tags can drive a tagged-union style of value. Rather than declaring an enumeration and a variant, the tags are written directly into the type of each constructor. A visitor can then dispatch on the tag using `when<>`.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

int main ()
{
  auto make_num = [](int v)  { return dsl::leaf<"num">(v); };
  auto make_str = [](std::string s) { return dsl::leaf<"str">(s); };

  auto v1 = make_num(42);
  auto v2 = make_str("hello");

  auto describe = [](auto const &node) {
    return dsl::match(node.tag(),
      dsl::when<dsl::pattern<"num">>([&](auto){ return "a number"; }),
      dsl::when<dsl::pattern<"str">>([&](auto){ return "a string"; }),
      dsl::otherwise([](auto){ return "something else"; })
    );
  };

  std::cout << describe(v1) << "\n";  // a number
  std::cout << describe(v2) << "\n";  // a string
}
```

The tags `"num"` and `"str"` are compile-time facts about the two `leaf` calls. The `describe` function recovers them at runtime via `node.tag()`, which returns the `std::string_view` that `leaf` stored from `Tag.view()`. The `when<>` clauses then match against that view using the patterns, which are themselves `FixedString`-backed.

This is the central pattern of DSLtk: compile-time names flow through `view()` into runtime strings, and runtime strings are matched back against compile-time names. `FixedString` is the hinge that makes the round trip possible.

## Limitations

The first limitation is that `N` is part of the type. A `FixedString<4>` and a `FixedString<5>` are unrelated types, and there is no common base that holds a tag of arbitrary length. In practice this is invisible, because the constructor deduces `N` from the literal and the user never writes the length explicitly. It does mean, however, that a function cannot accept "any `FixedString`" without making `N` a template parameter of its own.

```cpp
template <std::size_t N>
void process (dsl::FixedString<N> tag);   // accepts any length
```

The second limitation is that the bytes are copied into the type. A `FixedString<100>` carries one hundred bytes in its `data` array, and every copy of the value copies those bytes. For the short tags typical of a grammar this is negligible, but it does mean that `FixedString` is not a good vehicle for large compile-time blobs. The type is designed for identifiers, not for payloads.

The third limitation is fundamental to the NTTP mechanism: the string must be a literal known at compile time. A `FixedString` cannot be constructed from a `std::string`, from user input, or from any value that is not a constant expression. This is not a deficiency of `FixedString`; it is the defining property of a non-type template parameter. Runtime-built strings must be carried by other means, such as the `ASTNode`'s label or the payload of a `leaf`.

```cpp
std::string user_tag = read_from_config();
// dsl::leaf<user_tag>(42);   // error: user_tag is not a constant expression
```

The fourth limitation follows from the structural-equality rule: two tags compare equal as NTTPs only if their bytes match exactly, including the terminator. Padding or trailing spaces will produce distinct template specializations. Users should therefore write tags as bare literals without extraneous whitespace.

## Relationship to Standard Proposals

The C++ standardization committee has, over several cycles, considered a `fixed_string` type that would formalize the pattern represented here by `FixedString`. The most visible appearance of the idea in production code is `std::format`'s use of a fixed-string type to carry format strings as compile-time constants, enabling compile-time validation of format specifiers. `FixedString` is a minimal, self-contained realization of the same idea, tuned for the specific needs of an embedded-DSL library.

The standard proposals differ from `FixedString` in detail — they may support trailing capacity, constexpr iteration, or integration with `std::string` — but the core principle is identical: a literal class type with a `char` array, usable as an NTTP, with `constexpr` access to its contents. Should a standardized `fixed_string` arrive, it would be a drop-in replacement for `FixedString` at the template-argument position, and the rest of DSLtk would be unaffected.

Until then, `FixedString` provides the same service with a ten-line definition and no dependencies beyond the standard library headers that `DSLtk.hpp` already includes. Its smallness is a feature: it makes the type easy to read, easy to reason about, and easy to verify against the structural-type rules of C++20.

## Summary

`dsl::FixedString<N>` is a literal class type whose single non-static data member is a `char` array of size `N`. Its constructor deduces `N` from a string literal via a reference-to-array parameter and copies the literal, terminator included, into the array with `std::copy_n`. The `view()` method returns a `std::string_view` of length `N - 1`, and a defaulted `operator==` provides structural equality. Every member is `constexpr`, so the type is usable in any constant-expression context.

Because `FixedString` is a literal class type with structural members, it qualifies as a C++20 class-type non-type template parameter. This is what allows string literals to appear inside angle brackets throughout DSLtk: `dsl::leaf<"num">`, `dsl::node<"add">`, `dsl::pattern<"[0-9]+">`, `dsl::lit<"_km">`, `dsl::rule<"name">`, the keys of `when<>`, and the `add_rule<"...">` calls of the PEG layer all depend on this property. The tag is a compile-time fact carried in the type; its `view()` is the runtime string recovered from it.

The type carries the usual limitations of an NTTP carrier: `N` is part of the type, the bytes are copied into the type, and only string literals — not runtime strings — may be used as arguments. Within those limits, `FixedString` is the mechanism that lets DSLtk spell a grammar in source code rather than in configuration. The remaining chapters of this manual use it freely, and this chapter is the foundation for all of them.
