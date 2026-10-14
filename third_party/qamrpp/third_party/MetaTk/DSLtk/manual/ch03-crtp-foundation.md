# Chapter 3: The CRTP Foundation: `dsl::DSL<Derived, Features...>`

The previous two chapters introduced DSLtk's purpose and walked through a first
compilable program. This chapter opens the black box at the centre of the
library: the `dsl::DSL` base class. Every user-defined DSL inherits from it, and
every feature the toolkit provides enters your class through it.

Understanding this one class is the key to everything that follows. The class is
short — under a dozen lines of code — yet it encodes the design philosophy of
Chapter 1 in concrete syntax. Once its mechanics are clear, the rest of the
manual becomes a tour of individual features rather than a tour of new
machinery.

This chapter explains the Curiously Recurring Template Pattern, the variadic
mixin expansion that composes features, the `self()` accessor, and the contract
the base class establishes with its derived types. It also covers the practical
concerns of authoring a DSL: object layout, the pitfalls that catch new users,
and the contrast with classical virtual-function inheritance.

## What the Curiously Recurring Template Pattern Is

The Curiously Recurring Template Pattern, or CRTP, is a C++ idiom in which a
class inherits from a template instantiated on the class itself. The derived
class passes its own name as a template argument to the base. The base therefore
knows the exact type of the class that derives from it, even though inheritance
ordinarily works in the opposite direction.

A minimal sketch of the pattern captures its shape independently of DSLtk:

```cpp
template <typename Derived>
struct Base {
    void interface() {
        static_cast<Derived&>(*this).implementation();
    }
};

struct Concrete : Base<Concrete> {
    void implementation() { /* domain code */ }
};
```

The base exposes a method called `interface`. Inside that method it casts
`*this` to a reference to `Derived` and calls `implementation` on the result.
Because `Concrete` is the template argument, the cast resolves to the correct
type at compile time.

There is no virtual function anywhere in this design. The compiler sees a
concrete call to `Concrete::implementation` whenever `interface` is called on a
`Concrete` object. That call can be inlined, devirtualised, and optimised like
any ordinary method call.

CRTP thus inverts the usual direction of polymorphism. In classical
object-oriented design the base defines an interface and the derived type
supplies the implementation through a virtual override. In CRTP the base *uses*
the derived type, and the derived type *is* the concrete implementation. The
relationship is fixed at compile time and expressed entirely in the type system.

The pattern appears throughout the standard library and in many high-performance
libraries. It is the conventional way to obtain polymorphic behaviour without
paying for a vtable. DSLtk uses it for exactly that reason: every feature method
is a direct call, with no indirection at run time.

## Why DSLtk Uses CRTP Instead of Virtual Functions

The header's design notes state the rationale in a single line: CRTP avoids
virtual dispatch, so all polymorphism is compile-time. That one decision shapes
the rest of the library, and it is worth unpacking in detail.

A virtual function call goes through a vtable. Each object of a polymorphic type
carries a hidden pointer to that table, and each call reads the pointer, looks
up the slot, and branches. The indirection is small per call, but it adds up. It
is also opaque to the optimiser: the compiler cannot easily see which function a
slot will resolve to, so inlining is defeated.

For a library that exists to compose many small features — a pipe stage here, a
predicate there — that cost would dominate. A pipeline of ten stages would cost
ten indirect calls. A composed predicate would cost several more. CRTP removes
every one of those indirections by binding each call to a concrete type at
compile time.

With CRTP, the base class is templated on the derived type, so the compiler
instantiates a fresh `DSL<Derived, ...>` for each user DSL. Every method
inherited from every mixin becomes a method of a concrete `Derived` object.
Calls resolve statically and inline freely, because the compiler sees no
indirection to hide.

The second reason is orthogonality. The design notes observe that features are
orthogonal: adding `dsl::Pipeline` does not affect `dsl::PatternMatch`. Multiple
inheritance of mixins gives this property directly. Each feature contributes an
independent base subobject whose methods do not collide with any other
feature's.

A virtual-function design would require a single inheritance hierarchy with a
shared root, or else a complex interface scheme. Either choice would couple
features that have nothing to do with each other. The mixin approach keeps them
separate by construction, because each feature's methods live in its own base
subobject that the compiler can reason about in isolation.

The third reason is type precision. Because each `DSL<Derived, ...>` is a
distinct instantiation, every type error in feature usage is caught at compile
time against the concrete derived type. There is no base pointer through which a
mistyped call could slip to run time. The compiler works with exact types
throughout, which is exactly what a DSL author wants when the language's
semantics should be checked, not guessed at.

## The DSL Base Class in Full

The whole `dsl::DSL` class is short enough to quote in full. It is reproduced
below exactly as it appears in the header, Doxygen comment included:

```cpp
/**
 * @brief CRTP base class for all user-defined DSLs.
 *
 * Inherit from this to compose features:
 *
 *   struct MyDSL : dsl::DSL<MyDSL, dsl::Pipeline, dsl::Operators> { ... };
 *
 * The Derived type gets access to all methods provided by each Feature.
 * Features are mixed in via multiple inheritance through FeatureMixin<>.
 *
 * @tparam Derived  The user's DSL class (CRTP pattern).
 * @tparam Features Zero or more feature tags (Pipeline, Operators, etc.).
 */
template <typename Derived, typename... Features>
struct DSL : Features::template Mixin<Derived>...
{
protected:
  constexpr Derived &
  self () noexcept
  {
    return static_cast<Derived &> (*this);
  }
  constexpr const Derived &
  self () const noexcept
  {
    return static_cast<const Derived &> (*this);
  }
};
```

The class takes two kinds of template parameter. The first, `Derived`, is the
user's own DSL type. The second, `Features...`, is a variadic pack of feature
tags. Everything the base does flows from how these two parameters are used.

The class has no data members of its own. Its body contains only the two
protected `self()` overloads. All of its real work happens in the
base-specifier list — the part after the colon — which is examined in the next
section.

That base-specifier list is where every feature enters the class. It reads
`Features::template Mixin<Derived>...`, and the ellipsis expands the pack. The
result is a class that multiply inherits from each feature's nested
`Mixin<Derived>` template, acquiring the union of their methods.

This is the entire mechanism. The base class itself contributes no behaviour
beyond the `self()` accessor; behaviour comes from the mixins it pulls in. The
pattern is deliberately minimal so that the feature set stays open-ended: new
features can be added to the library without changing this class.

Above the class definition, the header forward-declares every feature tag. These
forward declarations let the `DSL` template refer to the tags before they are
fully defined:

```cpp
struct Pipeline;
struct Operators;
struct PatternMatch;
struct AST;
struct Rewrite;
struct ExprTemplates;
struct CustomLiterals;
struct Memoization;
struct LazyFeature;
struct Monadic;
struct ResultFeature;
struct CombinatorParser;
struct TaskPipeline;
```

Each of these tags is defined later in the header, and each carries a nested
`Mixin<Derived>` template that supplies the feature's methods. Chapter 5 covers
the tags and mixins in detail; this chapter treats them as opaque ingredients
that the base class composes.

## The Variadic Mixin Expansion

The most important line in the class is the base-specifier list. It is short
enough to examine in isolation:

```cpp
template <typename Derived, typename... Features>
struct DSL : Features::template Mixin<Derived>... {
    // body elided
};
```

When you write `dsl::DSL<MyDSL, dsl::Pipeline, dsl::Operators>`, the pack
`Features...` contains `dsl::Pipeline` and `dsl::Operators`. The compiler
expands the pattern `Features::template Mixin<Derived>...` by substituting each
element of the pack in turn, producing one base class per feature.

After expansion, the class looks as if it had been written with two explicit
bases. The equivalent hand-written form would be:

```cpp
struct DSL : Pipeline::Mixin<MyDSL>, Operators::Mixin<MyDSL> {
    // body elided
};
```

Each feature tag exposes a nested template called `Mixin`, parametrised on the
derived type. The `template` keyword is required because `Features` is a
dependent parameter pack; without it the compiler would not know that `Mixin`
names a template rather than a member or a specialisation.

This pattern generalises to any number of features. Writing
`dsl::DSL<MyDSL, A, B, C, D>` produces four mixin bases, one per feature.
Writing `dsl::DSL<MyDSL>` with no feature tags produces no mixin bases at all —
an empty class with only the `self()` accessor.

The expansion is purely structural. It happens at compile time and produces no
run-time code of its own. The compiler simply sees a class with several bases,
each of which is a concrete template instantiation whose methods are inherited
like any other.

Because `DSL` inherits from every `Mixin<Derived>` in the expansion, the derived
class acquires the methods of all of them at once. This is ordinary C++
multiple inheritance applied to a deliberate purpose: the union of feature
methods becomes the public surface of the user's DSL.

A feature tag like `dsl::Pipeline` does not itself contain methods. It contains
a nested `Mixin<Derived>` that contains the methods. The tag is a handle; the
mixin is the implementation. The `DSL` template bypasses the tag and goes
straight to the mixin, so the user never writes `Pipeline::Mixin` directly.

Consider what the pipeline and operators mixins contribute. The pipeline mixin
adds a static `wrap` method that lifts a value into a chain. The operators
mixin adds a static `make_pred` factory that builds composable predicates. When
both are mixed in, the derived class has both `wrap` and `make_pred` as static
members, available by simple name lookup:

```cpp
struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

auto v  = BasicDSL::wrap(5);                          // from Pipeline::Mixin
auto gt = BasicDSL::make_pred([](int x){ return x > 5; }); // from Operators::Mixin
```

The two methods coexist because they live in two different base subobjects.
There is no name clash because the method names differ. Even when two features
contributed methods with the same name, C++'s usual overload and hiding rules
would apply — but DSLtk's features are designed so that their public method
names do not collide.

Because each mixin is parametrised on `Derived`, a mixin's methods can refer to
the eventual concrete type. This is what makes the two-way contract described
later in this chapter possible: a mixin can call back into `Derived` for any
domain-specific data it needs, through `self()` or through direct name lookup.

This is the whole source of feature composition in DSLtk. There is no
registration step, no macro, and no run-time list of features. You name the
features in angle brackets, and the base class becomes the union of their
mixins. The order of features in the argument list does not affect which methods
are available; the class inherits from every mixin regardless of order.

## The self() Accessor

The `DSL` base class defines two protected member functions named `self()`. They
are the only methods the base class defines itself, as opposed to inheriting
from the mixins:

```cpp
constexpr Derived &self() noexcept {
    return static_cast<Derived &>(*this);
}
constexpr const Derived &self() const noexcept {
    return static_cast<const Derived &>(*this);
}
```

Each overload returns a reference to `Derived` by downcasting `*this`. The
non-const overload returns a non-const reference; the const overload returns a
const reference. Both are declared `constexpr` and `noexcept`, reflecting the
library's bias toward compile-time reasoning wherever practical.

The cast is safe because of how CRTP is set up. The `DSL<Derived, ...>` base is
a subobject of every `Derived` object, so a `static_cast` from the base
reference to a `Derived` reference is well-defined for any actual `Derived`
instance. The pattern relies on the user passing the correct derived type as the
first template argument, a discipline covered later in this chapter.

The accessor exists for the benefit of the mixins. A mixin inherits from `DSL`
only indirectly, through the derived class, but a mixin's instance methods may
need to reach the concrete `Derived` object that owns them. Calling `self()`
from within such a method returns that object, typed exactly as `Derived`.

Because `self()` is protected, it is visible to the mixins (which are bases of
`Derived`) but not to external code. Users of a DSL never call `self()` directly.
It is an implementation detail of the feature machinery, kept out of the public
interface on purpose.

Most user-facing feature methods are static, so they do not need `self()` at
all. The accessor matters for the smaller set of feature methods that operate
on an instance and need to read or modify the derived object's state. Those
methods use `self()` to obtain the derived reference in a uniform way, without
each feature reinventing its own downcast.

The `constexpr` qualifier matters because it permits the accessor to participate
in constant-evaluated code. The `noexcept` qualifier matters because the cast
itself cannot throw. Both qualifiers let the compiler reason more precisely
about code that uses the accessor, which in turn lets the optimiser treat calls
through `self()` as ordinary direct calls.

## The Two-Way Contract Between Base and Derived

The relationship between `dsl::DSL` and its derived class is not
one-directional. The base hands methods *to* the derived class through the
mixins, and some mixins reach *back into* the derived class for required
members. This two-way flow is the structural heart of the toolkit.

This contract is how features stay orthogonal yet cooperate with user code. The
mixin does not invent domain data; it expects the derived class to provide it.
The derived class does not implement feature logic; it provides hooks that the
mixin's logic consumes. Each side has a single, well-defined responsibility.

Three named hooks appear across the feature set. The rewrite feature reads
`Derived::rules` to find the rewrite rules of the language. The
custom-literals feature reads `Derived::literals` to find the registered literal
suffixes. The expression-templates feature calls `Derived::expr_value()` to read
the numeric payload of a value. Each hook is documented by its feature and
covered in its own chapter.

```cpp
struct VecDSL : dsl::DSL<VecDSL, dsl::ExprTemplates> {
    std::vector<float> data;
    const std::vector<float>& expr_value() const { return data; }
};
```

In this example, `expr_value()` is the hook that `dsl::ExprTemplates` requires.
The mixin supplies the arithmetic operators; the derived class supplies the data
those operators read. Neither side knows the other's internals; each knows only
the named contract member.

A feature that needs no domain data imposes no contract at all. `dsl::Pipeline`
and `dsl::Operators` contribute only static methods, so they impose no
requirements on `Derived`. You can mix them into any struct without adding any
members, which is exactly why the empty-bodied `BasicDSL` of the first example
works.

When a feature *does* need data, it documents the hook it expects, and the
derived class provides it. The contract is therefore opt-in per feature: only
the features you list can ask your class for anything, and each asks only for
the specific member it documents.

The contracts are named and explicit. They are not interface methods in the
object-oriented sense; they are members the mixin looks up by name at compile
time. If a required hook is missing, the error surfaces at instantiation as a
missing-member diagnostic, which is precise and local. Chapters 13, 15, and 17
cover the specific contracts of the rewrite, expression-template, and
custom-literal features in full.

## Compile-Time Polymorphism versus Runtime Dispatch

The difference between CRTP polymorphism and virtual-function polymorphism is
the difference between compile-time and run-time binding. DSLtk chooses the
former throughout, and the consequences are worth stating plainly.

With virtual functions, the call site does not know which function will run
until the program executes. The type of the object — or rather, the vtable it
points to — determines the target. This enables heterogenous containers and
runtime flexibility, at the cost of an indirection on every call.

With CRTP, the call site knows the exact type at compile time. Each
instantiation of `DSL<Derived, ...>` is a distinct type with its own copies of
the feature methods. The compiler can resolve every call statically, inline
every method body, and eliminate dead code with full knowledge of the target.

```cpp
struct A : dsl::DSL<A, dsl::Pipeline> {};
struct B : dsl::DSL<B, dsl::Pipeline, dsl::Operators> {};

// A and B are unrelated types. They share no common base that you can
// point to polymorphically. Each has its own wrap() resolved at compile time.
auto va = A::wrap(1);
auto vb = B::wrap(1);
```

`A` and `B` have no common runtime base. They are not interchangeable, and you
cannot store them in a single container of pointers to a shared `dsl::DSL` root.
That root does not exist as a polymorphic type; it exists only as a template,
and each instantiation is a separate concrete class.

This is an intentional trade-off. DSLtk DSLs are concrete value types. You
author one DSL per language, instantiate it, and use it directly. You do not
typically need to swap implementations at runtime, and the library does not
spend effort supporting that case.

The payoff is performance and type safety together. Every feature method is a
direct call. Every type error is caught at compile time against the exact
derived type. There is no vtable to populate, no RTTI to carry, and no way for a
runtime type mismatch to slip through the type system.

## The Three-Step Loop Through the CRTP Lens

Chapter 1 introduced a three-step loop for authoring a DSL: inherit `dsl::DSL`,
add domain members, and express logic through the operators the features
provide. With the CRTP mechanics now in view, each step can be restated in terms
of what the machinery actually does.

The first step is to inherit `dsl::DSL`, passing your class as the first
template argument and the features you want as the rest. This single line of
inheritance is the CRTP scaffolding: it gives the base class the derived type,
and it triggers the mixin expansion that injects the feature methods into your
class.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::Pipeline, dsl::Operators> {
    // domain-specific members go here
};
```

The second step is to add whatever domain-specific members your language needs.
These members may be data — a vector of tokens, a current state, a configuration
map — or methods that the feature mixins expect as hooks, such as `expr_value()`
or `rules`. From the CRTP perspective, these members become part of `Derived`,
and the mixins can reach them through `self()` or through name lookup.

The third step is to express the language's logic using the operators and
helpers the features provide. The pipe operator, the predicate combinators, the
`match` table, and the arithmetic operators are all available because the mixins
supplied them. You write the language at the level of *using* these tools, not
at the level of *implementing* them.

Each step corresponds to a layer of the mechanism. Inheritance sets up the type
graph. Member addition populates the derived type with domain data and contract
hooks. Operator usage exercises the mixin methods against that data. The three
steps are not arbitrary authoring advice; they map directly onto what the CRTP
machinery does.

This is why the loop is the same whether the language is a state machine, a
query builder, or a vector algebra. The features differ, but the three layers —
type, data, expression — are fixed by the base class. Chapter 6 and Chapter 7
show the loop applied to the pipeline and operators features in detail.

## Authoring a Minimal DSL

A minimal DSL is one that inherits `dsl::DSL` with one or more features but adds
no body of its own. Even with an empty body, the class already has behaviour,
because the mixin expansion injects real methods that the compiler binds at
compile time.

The smallest useful DSL mixes in a single feature. The body stays empty, yet the
class is already usable as a pipeline entry point:

```cpp
struct PipeDSL : dsl::DSL<PipeDSL, dsl::Pipeline> {};

auto v = PipeDSL::wrap(7)
       | dsl::pipe([](int x){ return x * x; })
       | dsl::pipe([](int x){ return x - 1; });
// v == 48
```

`PipeDSL` has no data members and no user-written methods. Yet it has a static
`wrap` method, contributed by `dsl::Pipeline::Mixin<PipeDSL>`, and it can
participate in pipe chains through the free-function `operator|` in the `dsl`
namespace.

A second minimal DSL mixes in only the operators feature. As before, the empty
body is enough to obtain a working predicate algebra:

```cpp
struct PredDSL : dsl::DSL<PredDSL, dsl::Operators> {};

auto positive = PredDSL::make_pred([](int x){ return x > 0; });
auto small    = PredDSL::make_pred([](int x){ return x < 10; });
auto in_range = positive & small;
// in_range(5) == true, in_range(-1) == false, in_range(20) == false
```

The body is empty here too. The `make_pred` factory comes from
`dsl::Operators::Mixin<PredDSL>`. The composed predicate `in_range` is built
with the overloaded `operator&`, which works on the predicate type that
`make_pred` returns and produces another predicate of the same kind.

A third minimal DSL mixes two features together. This reproduces the very first
example of the manual, showing that feature composition needs no glue code:

```cpp
struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

auto v    = BasicDSL::wrap(5)
          | dsl::pipe([](int x){ return x + 3; })
          | dsl::pipe([](int x){ return x * 2; });
auto gt5  = BasicDSL::make_pred([](int x){ return x > 5; });
auto even = BasicDSL::make_pred([](int x){ return x % 2 == 0; });
// v == 16, (gt5(v) && even(v)) == true
```

This is exactly the program in `examples/01-basic-usage.cpp`. The whole program
— pipe chain plus predicate algebra — rests on the empty-bodied struct and the
methods its two mixins inject. Nothing else is required.

The lesson is that the body of a DSL is optional. You add members only when your
domain needs them, and the features that need no hooks impose no obligation to
add anything. The base class and its mixins already provide a working surface
before you write a single line of your own.

## Why Derived Must Be the First Argument

The first template argument to `dsl::DSL` plays a special role that the others
do not. It is the *recurring* part of the Curiously Recurring Template Pattern,
and the whole mechanism depends on it being the user's own type.

The `self()` accessor casts `*this` to `Derived`. If `Derived` is not the actual
type of the object, the cast is undefined behaviour. The mixin expansion also
feeds `Derived` into each `Mixin<Derived>` template, so that mixin methods can
reach back into the correct concrete type when they need domain data.

Passing the wrong type as the first argument is therefore not a benign mistake.
It produces a class whose `self()` returns a reference to a type that is not the
object's actual type. Calls through that reference invoke methods on the wrong
object layout, with unpredictable results.

```cpp
// WRONG: the first argument is not the type being defined.
struct Wrong : dsl::DSL<dsl::Pipeline, dsl::Operators> {};
// The expansion treats dsl::Pipeline as Derived, so each mixin is
// instantiated over a feature tag rather than over Wrong. This does
// not produce the intended class and will not compile in practice.
```

In practice this mistake surfaces as a compile error, because the mixin's
`Mixin<Derived>` instantiation expects `Derived` to provide certain nested types
or methods that a feature tag does not supply. In the rare case where it does
compile, the resulting code has undefined behaviour at the first `static_cast`
inside `self()`.

The discipline is therefore simple: the first argument must always be the name
of the struct being defined. Every example in this manual and every example in
the source distribution follows that rule. C++ permits naming the struct inside
its own base-specifier list because the name is injected at the point of
declaration, which is what makes the pattern expressible at all.

Because the first argument is the only one with this special role, the remaining
feature arguments can be listed in any order. Only `Derived` is fixed in
position. The features that follow it form an unordered set from the type
system's point of view; their order affects neither the available methods nor
the layout of the resulting class.

## Object Size and Layout Considerations

The `DSL` base class has no data members, and many feature mixins add only
static methods. Static methods do not contribute to object size. A DSL whose
features contribute only static methods therefore has no data members anywhere
in its inheritance chain, and an empty class in C++ has size one.

```cpp
struct EmptyDSL : dsl::DSL<EmptyDSL, dsl::Pipeline, dsl::Operators> {};
// EmptyDSL has no data members of its own; Pipeline and Operators
// contribute only static methods. The empty-base optimisation keeps
// the object size at the minimum for any C++ object.
```

The single byte is the conventional size C++ assigns to any object with no data
members, so that distinct objects have distinct addresses. The base and mixin
subobjects participate in the empty base optimisation, so they add no padding of
their own to the complete object.

A DSL that adds data members of its own has a size determined by those members
plus the optimised-away bases. The mixin subobjects continue to contribute
nothing to the size:

```cpp
struct StatefulDSL : dsl::DSL<StatefulDSL, dsl::Pipeline> {
    int counter{};
    std::string label{};
};
// sizeof(StatefulDSL) is governed by counter and label; the mixin
// bases contribute nothing to the size beyond the empty-base case.
```

The mixin bases still contribute nothing to the size here. The size is entirely
a function of the user's own data members. This is reassuring: mixing in more
static-method features does not make objects larger, so adding a feature for its
methods alone is genuinely free at run time.

Some features do add data members to their mixin, and those features increase
object size by the size of those members. The general principle is that a
feature's storage cost is local to that feature: it imposes only what its mixin
declares, and nothing else. No feature pays for another feature's data.

This locality matters when DSL objects are stored in containers or returned from
functions. A DSL with only static-method features behaves as a zero-cost tag
type. A DSL with data members behaves as a plain value type carrying exactly
those members. Either way, the cost is what you see in the struct body, nothing
more.

## Common Authoring Pitfalls

A handful of mistakes account for most first-day problems with the base class.
Recognising them by their error messages shortens the learning curve
considerably.

The first is forgetting to pass `Derived` as the first template argument, as
covered in the preceding section. The fix is mechanical: the first argument is
always the name of the struct being defined. If a compile error mentions a
feature tag where a type was expected, this is almost always the cause.

The second is expecting a feature's methods to exist when the feature was not
listed. Each feature contributes methods only when it appears in the angle
brackets. A DSL that mixes in only `dsl::Pipeline` does not have `make_pred`,
because that method belongs to `dsl::Operators`:

```cpp
struct OnlyPipe : dsl::DSL<OnlyPipe, dsl::Pipeline> {};

// auto bad = OnlyPipe::make_pred([](int x){ return x > 0; });
// ERROR: 'make_pred' is not a member of 'OnlyPipe'.
// Fix: add dsl::Operators to the feature list.
struct PipeAndPred : dsl::DSL<PipeAndPred, dsl::Pipeline, dsl::Operators> {};
auto good = PipeAndPred::make_pred([](int x){ return x > 0; });
```

The compiler error for this case is usually clear: the method name is not a
member of the class. The fix is to add the missing feature to the list.
Recognising which feature owns which method is a matter of reading the relevant
chapter and the header's overview comment.

The third pitfall is expecting runtime polymorphism between different DSL types.
Two DSLs that happen to share a feature are not interchangeable. They are
distinct concrete types with no shared polymorphic base.

```cpp
struct A : dsl::DSL<A, dsl::Pipeline> {};
struct B : dsl::DSL<B, dsl::Pipeline> {};
// std::vector<dsl::DSL*> xs;  // no such polymorphic base exists
```

You cannot collect them under a common pointer and dispatch on a shared virtual
interface, because there is no such interface. This is the trade-off of
compile-time polymorphism, and it is deliberate. If heterogenous dispatch is
genuinely required, it must be built in the domain layer, not assumed from the
toolkit.

The fourth pitfall is omitting a hook that a feature requires. A feature that
reads `Derived::rules`, `Derived::literals`, or `Derived::expr_value()` will
fail to instantiate if that member is absent. The error appears at the point
where the mixin tries to use the missing name, and it points at the contract the
feature expects.

The fix is to add the missing member to the derived class. The feature's
documentation — and the relevant chapter of this manual — names the expected
member and its type. Adding it satisfies the contract and lets the mixin
compile. Chapter 13, Chapter 15, and Chapter 17 spell out the exact hooks for
the rewrite, expression-template, and custom-literal features.

## Contrast with Classic OOP Inheritance

To make the trade-off concrete, consider how the same composition might look
under classical virtual-function inheritance. A polymorphic design would
declare an abstract base with virtual methods, derive each feature as a
subclass, and combine features through multiple inheritance of those abstract
bases.

```cpp
// A sketch of the virtual-function alternative (NOT DSLtk's design).
struct IPipeline {
    virtual ~IPipeline() = default;
    virtual int wrap(int) const = 0;
};
struct IOperators {
    virtual ~IOperators() = default;
    virtual bool pred_check(int) const = 0;
};

struct VirtualDSL : IPipeline, IOperators {
    int wrap(int x) const override { return x; }
    bool pred_check(int x) const override { return x > 0; }
};
```

Every method is virtual in this sketch, every call goes through a vtable, and
every object carries one vptr per polymorphic base. The class can be used
polymorphically through `IPipeline*` or `IOperators*`, but at the cost of an
indirection per call and a larger object footprint.

DSLtk's design is the opposite. Methods are non-virtual, calls are direct, and
objects carry no vptrs for feature methods. The price is that the methods cannot
be dispatched through a polymorphic pointer to a shared base, because no such
base exists. Each `DSL<Derived, ...>` instantiation is its own concrete island.

For a DSL toolkit, the trade is one-sided. DSLs are typically instantiated as
concrete types and used directly within a single program. The flexibility of
runtime polymorphism is rarely needed, while the cost of virtual dispatch is
paid on every call. DSLtk therefore chooses static polymorphism and passes the
savings on to the user as inlined, optimisable code.

The virtual-function sketch above is not how DSLtk works, and the design notes
make clear it never will be. The sketch is included only to make the contrast
vivid. Once the trade-off is visible, the choice of CRTP reads as the natural
one for a library whose methods are small, numerous, and called often — exactly
the profile of a DSL's pipe stages, predicates, and tree traversals.

## Summary

The `dsl::DSL<Derived, Features...>` base class is the entire entry point to the
toolkit. It uses the Curiously Recurring Template Pattern to give the base class
compile-time knowledge of the derived type, and it uses variadic multiple
inheritance to inject every feature's mixin into the derived class.

The class is short: a pack expansion in the base-specifier list and two `self()`
overloads in the body. The expansion `Features::template Mixin<Derived>...`
produces one mixin base per feature, and the derived class becomes the union of
their methods. The protected `self()` accessor lets instance methods of mixins
reach back into the concrete `Derived` object when they need domain state.

The contract between base and derived is two-way but explicit. The base hands
methods to the derived class through the mixins; some features reach back into
the derived class for named hooks like `rules`, `literals`, and `expr_value()`.
A DSL whose features need no hooks can have an empty body and still exhibit real
behaviour, as the first example of the manual demonstrates.

The trade-off versus virtual-function inheritance is deliberate. DSLtk chooses
compile-time polymorphism, full inlining, and concrete value types over runtime
flexibility. The next chapter opens the first ingredient the base class relies
on for its string-taking features: the `FixedString` type that lets string
literals serve as non-type template parameters (see Chapter 4, FixedString:
Compile-Time Strings as Template Parameters).
