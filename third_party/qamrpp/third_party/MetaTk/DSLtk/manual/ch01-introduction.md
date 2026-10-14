# Chapter 1: Introduction and Design Philosophy

DSLtk is a single-header, header-only library for building Domain-Specific
Languages in modern C++. Its entire public surface lives in the `dsl`
namespace inside one file, `DSLtk.hpp`. It requires nothing beyond a C++20
compiler. There is no library to link and no package to fetch.

This chapter introduces the library's purpose, the design ideas that shape
every feature, and the mental model you will carry through the rest of the
manual. It is the only chapter that is purely conceptual. From Chapter 2 onward
the manual is hands-on, with compilable examples in every section.

A Domain-Specific Language, or DSL, is a small language tuned to one problem
domain. Configuration files, build rules, database queries, hardware
descriptions, and mathematical expression notations are all DSLs. Each is
deliberately narrow, and that narrowness is its strength.

When you build a DSL *inside* C++, you build an *embedded* DSL. You reuse the
host language's syntax, type system, and compiler while adding a thin,
expressive layer that reads almost like prose. The host language does the heavy
lifting; your layer provides the vocabulary.

DSLtk exists to make that layer easy to assemble from reusable pieces. The
central observation is that most embedded DSLs share the same handful of
building blocks. Pipes chain transformations. Predicates compose with logical
operators. Pattern dispatch tables route keys to handlers. Trees represent
syntax. Rewrite passes simplify trees. Lazy expressions fuse computation.

Rather than reinvent these blocks for every project, DSLtk packages each one
as an orthogonal *feature*. You compose exactly the features your language
needs and pay only for those you use.

The library is organised around one template. A DSL is a struct that inherits
`dsl::DSL`, passing itself as the first argument and the desired features as
the rest:

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::Pipeline, dsl::Operators> {
    // your domain members and methods
};
```

That single line of inheritance is the whole entry point. `MyDSL` is your
language class. The first template argument is the class itself — a recursive
technique known as CRTP, the Curiously Recurring Template Pattern.

Every remaining argument is a *feature tag*: `dsl::Pipeline`, `dsl::Operators`,
and so on. Each feature injects a small set of methods, operators, or nested
types into your class through a mixin. The result is a fully concrete object
with compile-time polymorphism and no virtual functions.

The features DSLtk ships are deliberately broad, so that one toolkit can serve
many kinds of language. The pipeline feature adds the pipe operator so values
flow through a chain. The operators feature gives you composable predicates
joined by `&`, `|`, and `!`.

The pattern-match feature builds compile-time dispatch tables from `when<>`
and `otherwise` clauses. The AST and rewrite features provide value-type syntax
trees and the passes that simplify them. The expression-templates feature
defers arithmetic into lazy trees that fuse on evaluation.

The custom-literals feature registers string suffixes. Several supporting
utilities round out the toolkit: memoization, lazy values, monadic optionals, a
`Result` type, parser combinators, task pipelines, and a Parsing Expression
Grammar engine.

Because every feature is a mixin, features combine without interfering. Adding
`dsl::Pipeline` to a class does not change how `dsl::Operators` behaves, and
neither affects `dsl::AST`. You can mix two features or ten.

This orthogonality is a direct consequence of the design. It is one of the
reasons the library stays small. It is also the reason the manual can devote one
chapter to each feature without overlap.

Let us look at the smallest possible DSL. The following class inherits two
features but adds no members of its own:

```cpp
#include "DSLtk.hpp"

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};
```

Even with an empty body, `BasicDSL` already has real behaviour. The pipeline
feature contributes a static `wrap` method that lifts a value into a chain.

The operators feature contributes a static `make_pred` factory. The pipe
operator itself is a free function in the `dsl` namespace, so it works on any
value paired with a `dsl::pipe` stage.

```cpp
auto v = BasicDSL::wrap(5)
       | dsl::pipe([](int x) { return x + 3; })
       | dsl::pipe([](int x) { return x * 2; });
// v == 16
```

Predicates compose with the logical operators you already know:

```cpp
auto gt5  = BasicDSL::make_pred([](int x) { return x > 5; });
auto even = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
auto rule = gt5 & even;
// rule(16) == true, rule(7) == false
```

That example is the entire `examples/01-basic-usage.cpp` program. It exercises
three ideas at once: feature composition, the pipe chain, and predicate
algebra. The rest of the manual unpacks each in depth.

## The Eight Design Pillars

The first design pillar is *static polymorphism through CRTP*. Traditional
object-oriented polymorphism uses virtual functions. A base class declares a
virtual method, derived classes override it, and a call through a base pointer
dispatches at run time through a vtable.

That dispatch costs an indirection and defeats inlining. CRTP instead lets the
base class know the derived type at compile time. The base casts `*this` to the
derived type, and the compiler sees a concrete call it can inline completely.

DSLtk uses this everywhere, so there is no virtual call anywhere in the
toolkit. The base class is short and worth quoting in full:

```cpp
template <typename Derived, typename... Features>
struct DSL : Features::template Mixin<Derived>... {
protected:
  constexpr Derived &self() noexcept {
    return static_cast<Derived &>(*this);
  }
  constexpr const Derived &self() const noexcept {
    return static_cast<const Derived &>(*this);
  }
};
```

The second pillar is *feature mixins through multiple inheritance*. Each
feature is a struct with a nested `Mixin<Derived>` template. `DSL` inherits
from every feature's mixin at once, so the derived class gains the union of all
their methods.

Because each mixin is parametrised on `Derived`, those methods can reach back
into your class when needed. The rewrite feature calls `Derived::rules`; the
literals feature reads `Derived::literals`. The contract between a feature and
the class that hosts it is therefore small and explicit.

The third pillar is *non-type template parameters carrying strings*. Before
C++20, you could not pass a string literal as a template argument directly.
DSLtk defines a small `FixedString<N>` type that wraps a compile-time char
array and qualifies as a non-type template parameter.

This lets you write `dsl::leaf<"num">(42)` and `dsl::pattern<"[0-9]+">`. The
tags and patterns live in the type system, enabling `constexpr` matching with
no run-time parsing of the string. Chapter 4 is devoted to `FixedString`.

The fourth pillar is *value semantics*. The library's data structures —
`ASTNode`, `Maybe<T>`, `Result<T,E>`, the expression-tree nodes — are value
types. They are copyable, movable, and stack-friendly.

You can return them from functions, store them in containers, and pass them by
value without thinking about ownership. Where heap is used it is hidden behind
a value interface, as with `ASTNode`'s internal vector of children.

The fifth pillar is *deferred evaluation*. Both `dsl::Lazy<T>` and the
expression templates postpone work. `Lazy<T>` runs a producer exactly once on
first access and caches the result.

Expression templates build a tree of operations as you write `a + b * c` and
perform a single fused evaluation when you call `.eval()`. Deferral lets you
describe computation cheaply and commit to it deliberately.

The sixth pillar is *explicit error flow*. DSLtk does not favour exceptions for
control flow. The `Result<T,E>` type models success and failure as data, with
`map`, `and_then`, and `map_err` to transform each branch.

The parser combinator core returns an `ExpectedResult<T>` that carries an
optional value *and* a structured `ParseError`. A failed parse is not an
exception but a value you inspect.

The seventh pillar is *compositionality through operators*. C++ lets you
overload operators, and DSLtk leans on this to make DSLs read naturally.

The pipe `|`, the logical `& | !` on predicates, the arithmetic `+ - * /` on
expression-templated values, and the `& | *` on parsers all build larger
abstractions from smaller ones using ordinary infix syntax. Operators are the
connective tissue of the library, not a gimmick.

The eighth pillar is *additive growth*. The most recent part of the toolkit, the
Parsing Expression Grammar subsystem, was layered on top of the existing
pattern-matching and parser-combinator machinery without modifying any earlier
type.

The library is structured so that new subsystems extend rather than reshape
what came before. This matters to you as a user because the documented
behaviour of an old feature will not quietly change when a new one arrives.

## What the Pillars Buy You

These eight pillars are not abstract. They are the reason a program against
DSLtk is short, fast, and type-safe at the same time.

CRTP and mixins give you composition *and* zero overhead together. These are
two goals that usually conflict; here they are reconciled by doing all the work
at compile time.

NTTP strings give you readable, compiler-checked tags *and* run-time
performance, because the string never has to be re-parsed at run time. Value
semantics give you safety *and* simplicity, because there are no dangling
pointers to reason about.

## Who DSLtk Is For

DSLtk is for library authors who want to ship a small embedded language without
pulling in a framework. It is for application developers who need a
configuration parser, a query builder, a rule engine, or a state machine
without leaving C++.

It is for compiler and tooling writers who need a parser combinator core and a
rewrite engine for ASTs. And it is for anyone who wants to see how modern C++ —
concepts, NTTPs, `constexpr`, variadic templates — comes together to build
something practical.

## What DSLtk Is Not

It is equally important to know what DSLtk is *not*. It is not a full
general-purpose parser generator like ANTLR. The PEG and combinator subsystems
are intentionally lightweight.

It is not a regex library. The `pattern<>` engine implements a small, fixed
subset of regular-expression syntax. It is not a threading library; the caching
and lazy helpers are deliberately single-threaded.

And it is not a replacement for the standard library's optionals and variants.
Rather, it composes with them, wrapping `std::optional` in `Maybe` and building
`Result` on top of `std::variant`.

## Requirements

The requirements are minimal and worth stating up front. You need a compiler
that implements C++20. The library uses concepts, class-type non-type template
parameters, `constexpr` lambdas, and modern standard-library idioms.

You need no external dependencies. Everything comes from the standard library
headers that `DSLtk.hpp` itself includes. And because the library is
header-only, there is nothing to link.

```cpp
#include "DSLtk.hpp"   // that is the whole dependency
```

The distribution provides a CMake `INTERFACE` library target so that projects
using CMake can consume DSLtk the idiomatic way. The target sets `cxx_std_20`
as a required compile feature and exports the header into an include directory.

Chapter 2 walks through both the CMake route and the plain-compiler route in
detail, including how to install the package and find it downstream.

## The Header's Own Quick Reference

The header itself opens with a long Doxygen-style comment that doubles as a
quick reference. It contains an overview, a quick start, a per-feature reference
with miniature examples, and a set of full examples labelled A through H.

This manual is the long-form counterpart to that comment. Where the header
gives a three-line sketch, the manual gives a chapter. Where the header lists a
signature, the manual explains the semantics, the trade-offs, and the idioms.

## The Mental Model

The mental model to carry forward is simple. A DSLtk program is a C++ program
in which you do three things.

First, you define a struct that inherits `dsl::DSL` with the features you want.
Second, you add whatever domain-specific members or methods you need. Third,
you express the language's logic using the operators and helpers those features
provide.

Everything else — the trees, the parsers, the pipes, the caches — is machinery
in service of that three-step loop. The loop is the same whether your language
is a state machine, a query builder, or a vector algebra.

## A Dispatch-Driven Example

Consider how the loop plays out for a state-machine language. You want states
and events with transitions driven by pattern matching.

You inherit both `dsl::Pipeline` and `dsl::PatternMatch`. You store a current
state as a member. You build a transition table with `dsl::match`. The features
hand you the pieces; you write the domain.

```cpp
struct SM : dsl::DSL<SM, dsl::Pipeline, dsl::PatternMatch> {
    enum class S { Idle, Running, Done };
    static constexpr auto transitions = dsl::match(
        dsl::when<S::Idle>   ([](auto e){ return e=="start" ? S::Running : S::Idle; }),
        dsl::when<S::Running>([](auto e){ return e=="stop"  ? S::Done    : S::Running; }),
        dsl::otherwise       ([](auto){ return S::Done; })
    );
    S state = S::Idle;
    void send(std::string_view e){ state = transitions(state, e); }
};
```

Sending events drives the state forward. After `sm.send("start")` the state is
`Running`; after `sm.send("stop")` it is `Done`. Two features and a handful of
lines implement a complete transition system.

## An Arithmetic Example

Or consider a vector arithmetic language built on expression templates. You
inherit `dsl::ExprTemplates`, hold a vector of floats, and expose an
`expr_value()` accessor.

The arithmetic operators are then supplied by the mixin, and evaluation fuses
into a single pass over the data.

```cpp
struct VecDSL : dsl::DSL<VecDSL, dsl::ExprTemplates> {
    std::vector<float> data;
    const std::vector<float>& expr_value() const { return data; }
};
// VecDSL a{{1,2,3}}, b{{4,5,6}};
// auto result = (a + b).eval();  // {5, 7, 9}
```

These two examples — one dispatch-driven, one arithmetic-driven — show the
range of languages DSLtk supports from the same base class. The features differ,
but the loop is identical.

## How the Manual Is Organised

The remainder of the manual is a systematic tour, organised into twelve parts.
Part I finishes the foundations: how to obtain and build the library, how CRTP
and mixins work, how `FixedString` enables string template parameters, and how
the feature tags and utility concepts are defined.

Parts II through XII then take each subsystem in turn. You will learn the pipe
and predicate algebras, the pattern matcher and its regex engine, the AST and
its rewrites, the expression templates, the literals, the caching and lazy
helpers, the monadic and result types, the parser combinator core, the task
pipeline, and finally the PEG grammar engine with its matcher.

Each chapter is self-contained enough to be read in isolation. At the same
time, chapters cross-reference their neighbours, so a reader who goes in order
builds a connected picture rather than a bag of tricks.

## A Note on the Code Listings

Every snippet in this manual uses the real `dsl::` API and is written to
compile against `DSLtk.hpp` with a conforming C++20 compiler. Where a snippet
is abbreviated for space — for instance, omitting `#include` lines or `main` —
that is noted.

The companion `examples/` directory in the source distribution contains
complete, compilable programs for nearly every feature. This manual frequently
echoes and expands on those examples, so you can read prose here and run code
there.

## A Brief Glossary

A few terms recur throughout the manual and are worth defining once. A *feature
tag* is one of the empty structs like `dsl::Pipeline` that you pass to
`dsl::DSL`.

A *mixin* is the nested `Mixin<Derived>` template inside a feature tag; it is
where the feature's methods actually live. A *clause* is one arm of a
`dsl::match` table, built with `dsl::when` or `dsl::otherwise`.

An *NTTP* is a non-type template parameter — a value, as opposed to a type,
passed between angle brackets. `dsl::leaf<"num">` and `dsl::pattern<"[0-9]+">`
both use NTTPs. A *fixpoint* is the state a rewrite pass reaches when no rule
fires any more; Chapter 14 covers this in detail.

## Expectations and Scope

DSLtk values clarity and composability above completeness. The pattern engine
implements a useful subset, not full POSIX regex. The parser combinators are a
teaching-quality Parsec, not an industrial parser toolkit. The PEG engine is
additive and lightweight.

Within its chosen scope, however, each piece is consistent, documented, and
designed to combine with the others. Understanding the design philosophy in
this chapter will make the concrete APIs in later chapters feel predictable
rather than arbitrary.

## Summary

DSLtk is a header-only C++20 toolkit for embedded DSLs, built on CRTP, feature
mixins, and NTTP strings. You compose features by inheriting `dsl::DSL`, and
the compiler does all the polymorphic work. The library favours value
semantics, deferred evaluation, explicit error flow, and operator-based
composition, and it grows additively rather than by rewriting itself.

The next chapter turns philosophy into practice. You will obtain the header,
compile a first program with and without CMake, run the bundled examples, and
learn to read the most common compile errors. From there, Chapter 3 opens the
`dsl::DSL` base class and shows exactly how one line of inheritance becomes a
fully featured language.
