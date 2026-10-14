# Chapter 5: Feature Tags, Mixins, and Utility Concepts

## The Feature Tag Idiom

Every non-trivial DSL built with DSLtk is assembled from a small set of reusable units called *feature tags*. A feature tag is a plain struct, typically empty or near-empty at its outer level, that exposes a single nested template:

```cpp
struct SomeFeature
{
  template <typename Derived> struct Mixin
  {
    // members injected into the user's DSL class
  };
};
```

The tag itself is what the user passes to `dsl::DSL`. The `Mixin` is what gets inherited. This split exists so that the public name of a feature (`dsl::Pipeline`, `dsl::Operators`, and so on) stays short and free of template parameters, while the actual injected members remain parameterized on the derived DSL type. The two-level naming is the load-bearing trick that lets features compose through multiple inheritance without name clashes on the tag itself.

The mechanism by which mixins are joined to the user's class is the CRTP base shown in Chapter 3. Its essential line is reproduced below:

```cpp
template <typename Derived, typename... Features>
struct DSL : Features::template Mixin<Derived>...
{
protected:
  constexpr Derived &self () noexcept { return static_cast<Derived &> (*this); }
  constexpr const Derived &self () const noexcept { return static_cast<const Derived &> (*this); }
};
```

`DSL` expands its `Features` pack by privately-or-publicly inheriting each `Feature::Mixin<Derived>`. The result is that a class declared as `dsl::DSL<MyDSL, dsl::Pipeline, dsl::Operators>` simultaneously derives from `Pipeline::Mixin<MyDSL>` and `Operators::Mixin<MyDSL>`. Whatever members those mixins declare become reachable on `MyDSL` itself, subject to access and overload resolution.

This chapter is the bridge between the CRTP foundation of Chapter 3 and the individual feature chapters that follow. Each feature tag listed here receives its own full treatment in a later chapter; the present chapter catalogues the shared idiom, the three categories of feature, the conditional-member contract, and the utility concepts that let user code introspect which features a DSL has mixed in.

## Categories of Feature

Although every feature tag shares the same syntactic shape, their mixins fall into three categories distinguished by what they contribute to the derived class.

The first category is the *pure marker* feature. Its `Mixin` body is empty; the tag exists only so a DSL can advertise that it "has" the feature. The second is the *method-injecting* feature, whose `Mixin` adds one or more member functions — `wrap`, `make_pred`, `make_leaf`, `clear_cache`, and so on. The third is the *operator-injecting* feature, which uses `friend` operators defined inside the mixin to wire up arithmetic or piping syntax for the derived type.

These three categories are not enforced by the type system; they are a descriptive taxonomy. A single feature tag could in principle mix all three behaviors, although in practice the library keeps them separated. Recognizing which category a given tag belongs to is the fastest way to predict what writing it into a `Features...` list will actually do to the resulting DSL.

The sections below take each category in turn, with concrete examples drawn verbatim from the header.

## Pure Marker Features

A pure marker feature has a `Mixin` whose body contributes nothing. The `PatternMatch` tag is the clearest example:

```cpp
struct PatternMatch
{
  template <typename Derived> struct Mixin
  {
    // Feature presence marker; no additional methods needed since
    // dsl::match/when/otherwise are free functions in the dsl:: namespace.
  };
};
```

The comment in the source explains the rationale: the actual functionality (`dsl::match`, `dsl::when`, `dsl::otherwise`) is delivered as free functions in the `dsl` namespace, so the mixin has nothing to inject. The tag still serves a purpose, because it lets a DSL declare intent and lets generic code check for that declaration through the `HasFeature` concept introduced later in this chapter.

Five further marker tags follow the same pattern. Each is a one-line declaration of an empty `Mixin`:

```cpp
struct LazyFeature    { template <typename Derived> struct Mixin {}; };
struct Monadic        { template <typename Derived> struct Mixin {}; };
struct ResultFeature  { template <typename Derived> struct Mixin {}; };
struct CombinatorParser { template <typename Derived> struct Mixin {}; };
struct TaskPipeline   { template <typename Derived> struct Mixin {}; };
```

These correspond to integrations whose machinery lives elsewhere — `Lazy<T>` (Chapter 19), the `Maybe<T>` type and monadic helpers (Chapters 20 and 21), `Result<T,E>` (Chapter 22), parser combinators (Chapters 23 through 25), and `Task`/`TaskChain` (Chapter 26). The tag on the DSL is what lets a generic library function gate itself on the presence of that integration, even when no methods are being injected.

A DSL can list any combination of these markers without paying any runtime cost. Because each `Mixin` is empty, the resulting derived class gains no data members and no virtual functions. The inheritance is, in effect, a compile-time annotation. Markers matter because they turn otherwise-invisible design intent into something the type system can query, as shown in the concepts section below.

## Method-Injecting Features: Pipeline and Operators

The `Pipeline` feature tag contributes a single static member, `wrap`, which lifts a value into the pipeable domain:

```cpp
struct Pipeline
{
  template <typename Derived> struct Mixin
  {
    template <typename T>
    static constexpr T
    wrap (T &&v)
    {
      return std::forward<T> (v);
    }
  };
};
```

`wrap` is, mechanically, a perfect-forwarding identity. Its job is not to transform the value but to provide a named entry point on the DSL so that downstream `operator|` stages, documented in Chapter 6, can be chained. Because `wrap` is `static` and `constexpr`, calling `MyDSL::wrap(10)` costs nothing at runtime and works in constant-evaluated contexts.

The `Operators` feature tag is structurally identical: one static factory, `make_pred`, which lifts a callable into the library's composable `Predicate` type:

```cpp
struct Operators
{
  template <typename Derived> struct Mixin
  {
    template <typename F>
    static constexpr auto
    make_pred (F &&f)
    {
      return dsl::predicate (std::forward<F> (f));
    }
  };
};
```

Predicates built this way compose with `operator&`, `operator|`, and `operator!` to form boolean expressions, as Chapter 7 describes in detail. Note that the mixin does not store any state of its own; it merely exposes a named constructor. This is the common shape of a method-injecting feature: a thin, often static, member function whose body forwards to a free function in the `dsl` namespace.

The basic-usage example shipped with the library combines both features and shows the typical call shape:

```cpp
struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

int main() {
  auto v = BasicDSL::wrap(5) | dsl::pipe([](int x) { return x + 3; })
           | dsl::pipe([](int x) { return x * 2; });
  auto gt5 = BasicDSL::make_pred([](int x) { return x > 5; });
  auto even = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
  std::cout << v << " " << (gt5(v) && even(v)) << "\n";
}
```

Because both injected members are static, the DSL need not even be instantiated to use them; the class name suffices. This is the idiomatic shape for stateless features.

## Method-Injecting Features: AST, Rewrite, CustomLiterals

The `AST` feature tag contributes two static factories, `make_leaf` and `make_node`, which are convenience wrappers around the free `dsl::leaf<Tag>` and `dsl::node<Tag>` functions covered in Chapter 11:

```cpp
struct AST
{
  template <typename Derived> struct Mixin
  {
    template <FixedString Tag, typename T>
    static ASTNode
    make_leaf (T &&val)
    {
      return dsl::leaf<Tag> (std::forward<T> (val));
    }

    template <FixedString Tag, typename... Children>
      requires (std::convertible_to<Children, ASTNode> && ...)
    static ASTNode
    make_node (Children &&...children)
    {
      return dsl::node<Tag> (std::forward<Children> (children)...);
    }
  };
};
```

Both factories take a `FixedString` non-type template parameter (Chapter 4) naming the node tag. `make_node` carries a fold constraint requiring every child to be convertible to `ASTNode`, which is the value type documented in Chapter 10. The mixin is otherwise stateless; like `Pipeline` and `Operators`, it only re-exports free functions under the DSL's own name.

The `Rewrite` and `CustomLiterals` tags take a different shape. Their mixins still inject methods, but the methods are guarded by a `requires` clause that depends on the derived class. That contract is important enough to deserve its own section, which follows shortly.

## Operator-Injecting Features: ExprTemplates

The third category uses `friend` operators defined inside the mixin to wire up expression syntax for the derived type. The `ExprTemplates` tag is the canonical example:

```cpp
struct ExprTemplates
{
  template <typename Derived> struct Mixin
  {
    template <typename R>
    friend constexpr auto
    operator+ (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::plus<>, Derived, R>{ lhs, rhs };
    }

    template <typename R>
    friend constexpr auto
    operator- (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::minus<>, Derived, R>{ lhs, rhs };
    }

    template <typename R>
    friend constexpr auto
    operator* (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::multiplies<>, Derived, R>{ lhs, rhs };
    }

    template <typename R>
    friend constexpr auto
    operator/ (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::divides<>, Derived, R>{ lhs, rhs };
    }

    friend constexpr auto
    operator- (const Derived &e)
    {
      return UnaryExpr<std::negate<>, Derived>{ e };
    }
  };
};
```

Each `friend` declaration is found through argument-dependent lookup only when one operand has type `Derived`. The operators return `BinExpr` or `UnaryExpr` expression-template nodes (Chapters 15 and 16) rather than eagerly computing a value, which is what makes the resulting expressions lazy.

The derived class is responsible for supplying an `expr_value()` member that returns the underlying scalar, as the source comment notes. Without that member, the eventual call to `eval()` on the built expression will not compile. The `ExprLike` concept introduced later in this chapter formalizes that expectation.

Because the operators are defined as friends *inside the mixin*, they are injected into the enclosing namespace only when the mixin is actually inherited. A DSL that does not list `dsl::ExprTemplates` in its feature list will not get the operators, even if it otherwise looks like a scalar wrapper. This is the precise mechanism by which operator overloading is kept opt-in.

## The Conditional-Member Contract

Some mixin methods only make sense when the derived class provides a particular static member. The library expresses this with a `requires requires` clause on the method itself. The `Rewrite` tag is the clearest example:

```cpp
struct Rewrite
{
  template <typename Derived> struct Mixin
  {
    ASTNode
    rewrite (const ASTNode &tree) const
      requires requires { Derived::rules; }
    {
      return Derived::rules.apply (tree);
    }
  };
};
```

The inner `requires { Derived::rules; }` expression checks that the derived class exposes a static member named `rules`. Only then is `rewrite` part of the mixin's overload set. A DSL that omits `rules` will not have a `rewrite` method at all — overload resolution will reject the call rather than producing a hard error inside the template body.

The `CustomLiterals` tag uses the same pattern with `Derived::literals`:

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

In both cases the derived class must define a static member of the appropriate type: `RewriteSet` for `rules`, `LiteralSet` for `literals`. The library does not constrain the *type* of that member through the concept, only its existence and name; the well-formedness of `rules.apply(tree)` or `literals.parse(input)` is checked separately when the method is actually invoked.

The mini-DSL example shipped with the library demonstrates the contract for `CustomLiterals`:

```cpp
struct MiniDSL : dsl::DSL<MiniDSL, dsl::Pipeline, dsl::CustomLiterals> {
  static constexpr auto literals = dsl::literal_set(
      dsl::lit<"_km">([](long double v) { return v * 1000.0L; }),
      dsl::lit<"_m"> ([](long double v) { return v; }));
};

int main() {
  auto meters = MiniDSL::parse_literal("2.5_km");
  auto shown = MiniDSL::wrap(meters) | dsl::pipe([](long double x) { return x + 10.0L; });
  std::cout << shown << "\n";
}
```

The `static constexpr auto literals` declaration satisfies the `requires` clause, which in turn makes `parse_literal` available as a static member of `MiniDSL`. The equivalent pattern for `Rewrite` is a `static inline auto rules = dsl::rewrite_set(...);` declaration, as Chapter 13 documents.

This idiom is deliberately conservative. The constraint fires only on the member name, not on its type, so users have latitude in how they construct the rule set or literal set. The trade-off is that a misspelled member name silently removes the method rather than producing a type error at the point of declaration.

## Method-Injecting Features: Memoization

The `Memoization` tag injects a single instance method, `clear_cache`, that delegates to a `memo_cache` member expected on the derived class:

```cpp
struct Memoization
{
  template <typename Derived> struct Mixin
  {
    void
    clear_cache ()
    {
      static_cast<Derived &> (*this).memo_cache.clear ();
    }
  };
};
```

Unlike `Pipeline::wrap` or `Operators::make_pred`, `clear_cache` is non-static and uses the CRTP downcast to reach the derived object. The expected `memo_cache` member is not constrained by a `requires` clause here; the body simply assumes it. Chapter 18 covers the `memoize` helper and the `MemoizedCallable` wrapper that typically produces such a cache.

This tag illustrates a subtle point about the method-injecting category: not every injected method is static. Instance methods that downcast to `Derived` are equally valid, and they are the natural shape when the injected behavior needs to mutate state held by the derived class itself.

## Forward Declarations and Definition Order

The header forward-declares every feature tag before any of them is defined. This is what allows the order of definitions in the header to differ from the order in which they are used: a mixin may reference another feature's machinery, or be referenced by it, without forcing a strict topological ordering on the file.

For the user, the practical consequence is that the entire feature-tag vocabulary is available as soon as `DSLtk.hpp` is included. There is no need to include sub-headers or to declare tags in any particular sequence. The following are all usable in any order in user code:

```cpp
struct A : dsl::DSL<A, dsl::Pipeline, dsl::Rewrite>          { static inline auto rules = dsl::rewrite_set(); };
struct B : dsl::DSL<B, dsl::CustomLiterals, dsl::ExprTemplates> {
  static constexpr auto literals = dsl::literal_set();
  double expr_value() const { return 0.0; }
};
struct C : dsl::DSL<C, dsl::AST, dsl::PatternMatch, dsl::Memoization> {};
```

The forward declarations are an implementation detail, but they shape how user code may be written. Because the tags are complete types by the time `DSL` is instantiated, mixins may freely refer to sibling tags in their own bodies, and users may freely order their feature lists however reads most naturally.

## How Features Compose

A DSL listing several feature tags gains the union of their mixins. The mechanism is the simple variadic expansion in the `DSL` base:

```cpp
template <typename Derived, typename... Features>
struct DSL : Features::template Mixin<Derived>...
{ /* ... */ };
```

Each listed tag contributes its `Mixin<Derived>` as one base class of `DSL`, and `DSL` itself is then a base of the user's class. The result is linear multiple inheritance over a handful of empty or near-empty bases, which modern compilers collapse to nothing in the common case.

A four-feature DSL looks like this in practice:

```cpp
struct RichDSL
  : dsl::DSL<RichDSL,
             dsl::Pipeline,
             dsl::Operators,
             dsl::AST,
             dsl::Rewrite>
{
  static inline auto rules = dsl::rewrite_set(
      /* ... rules ... */);
};

int main() {
  auto tree = RichDSL::make_node<"root">(RichDSL::make_leaf<"n">(42));
  RichDSL r;
  auto optimized = r.rewrite(tree);
  auto pred = RichDSL::make_pred([](int x) { return x > 0; });
  (void)optimized; (void)pred;
}
```

Here `RichDSL` simultaneously has `wrap` (from `Pipeline`), `make_pred` (from `Operators`), `make_leaf`/`make_node` (from `AST`), and `rewrite` (from `Rewrite`, gated on the `rules` member). The four features coexist without conflict because each contributes members under distinct names.

Composition is not free of caveats. Two features that inject identically-named members would produce an ambiguity at the derived class, which is why the library's feature set is designed with disjoint member names. Users writing their own mixins, as the next section illustrates, should follow the same convention.

## Building a Feature Tag: An Illustrative Example

The feature-tag pattern is intentionally simple, and the clearest way to demystify it is to write one. The code below is illustrative user code, not part of the library; it shows the minimum scaffolding needed to add a custom feature to a DSL.

```cpp
// User code — not part of DSLtk.
namespace dsl {
struct Greeter
{
  template <typename Derived> struct Mixin
  {
    template <typename T>
    static constexpr std::string
    greet (const T &who)
    {
      return "hello, " + std::string{who};
    }
  };
};
} // namespace dsl

struct GreetDSL : dsl::DSL<GreetDSL, dsl::Greeter> {};

static_assert(GreetDSL::greet("world") == "hello, world");
```

The tag is an empty struct. The `Mixin` is a class template with a single type parameter, conventionally named `Derived`. Members declared inside the mixin become members of any class that inherits `Greeter::Mixin<Derived>` through the `DSL` base.

A slightly richer version can demonstrate the conditional-member contract:

```cpp
// User code — not part of DSLtk.
namespace dsl {
struct VerboseGreeter
{
  template <typename Derived> struct Mixin
  {
    static std::string
    formal_greet (std::string_view who)
      requires requires { Derived::prefix; }
    {
      return std::string{Derived::prefix} + ", " + std::string{who};
    }
  };
};
} // namespace dsl

struct FormalDSL
  : dsl::DSL<FormalDSL, dsl::VerboseGreeter>
{
  static constexpr std::string_view prefix = "greetings";
};

static_assert(FormalDSL::formal_greet("stranger") == "greetings, stranger");
```

This mirrors the shape of `Rewrite` and `CustomLiterals` exactly: a method on the mixin, guarded by a `requires requires` clause naming a static member that the derived class must supply. The pattern is compositional precisely because it is so small; a custom feature is just another struct with a nested `Mixin` template.

## The Utility Concepts

The header provides five concepts that introspect which features a type has mixed in. They are defined verbatim as follows:

```cpp
template <typename T, typename F>
concept HasFeature = std::derived_from<T, typename F::template Mixin<T>>;

template <typename T>
concept Pipeable = HasFeature<T, Pipeline>;

template <typename T>
concept ExprLike = HasFeature<T, ExprTemplates>
                   && requires (const T &t) { t.expr_value (); };

template <typename T>
concept HasLiterals
    = HasFeature<T, CustomLiterals> && requires { T::literals; };

template <typename T>
concept Rewritable = HasFeature<T, Rewrite> && requires { T::rules; };
```

`HasFeature` is the foundation. It checks that `T` derives from `F::Mixin<T>` — exactly the relationship that the `DSL` base establishes. The four remaining concepts layer additional `requires` clauses on top: `Pipeable` is `HasFeature<T, Pipeline>` alone, while `ExprLike`, `HasLiterals`, and `Rewritable` add a check for the member that the corresponding feature needs to function.

The reason `ExprLike` also requires `t.expr_value()` is that the `ExprTemplates` mixin alone does not supply that member; it only injects operators. The derived class must independently provide `expr_value()` for the operators to eventually evaluate. Folding that requirement into the concept lets generic code treat any `ExprLike` type as a fully usable expression-template leaf without further checks.

`HasLiterals` and `Rewritable` fold in the same static member that the conditional-member contract checks for. This means the concepts and the `requires` clauses on the mixin methods are aligned: a type that satisfies `Rewritable` is guaranteed to have a callable `rewrite`, and a type that satisfies `HasLiterals` is guaranteed to have a callable `parse_literal`.

## Using the Concepts in Generic Code

The concepts are intended for use in user-written templates. The most common shape is a function template constrained on a single feature concept, so that the function body can rely on the feature's members being present.

A function constrained with `Pipeable` can assume that `wrap` is available:

```cpp
template <typename D>
  requires dsl::Pipeable<D>
auto
double_then_inc (int x)
{
  return D::wrap(x)
       | dsl::pipe([](int v) { return v * 2; })
       | dsl::pipe([](int v) { return v + 1; });
}
```

A function constrained with `ExprLike` can assume both the operators and `expr_value`:

```cpp
template <typename D>
  requires dsl::ExprLike<D>
auto
polynomial (const D &a, const D &b)
{
  return (a + b) * a - b;   // lazy BinExpr tree
}
```

A function constrained with `HasLiterals` can call `parse_literal`:

```cpp
template <typename D>
  requires dsl::HasLiterals<D>
long double
parse_with (std::string_view s)
{
  return D::parse_literal(s);
}
```

A function constrained with `Rewritable` can call `rewrite` on an instance:

```cpp
template <typename D>
  requires dsl::Rewritable<D>
dsl::ASTNode
optimize (const D &dsl, const dsl::ASTNode &tree)
{
  return dsl.rewrite(tree);
}
```

The generic `HasFeature` concept lets user code write feature-agnostic helpers, including over its own custom tags. The illustrative `Greeter` tag from earlier can be checked the same way:

```cpp
template <typename D>
  requires dsl::HasFeature<D, dsl::Greeter>   // user tag, library concept
std::string
say_hi (std::string_view who)
{
  return D::greet(who);
}
```

This is the mechanism by which marker features earn their keep. A DSL that lists `dsl::Monadic` advertises that it participates in monadic combinators, even though the tag injects no methods; user code can then write a template constrained with `HasFeature<D, dsl::Monadic>` to gate behavior on that advertisement.

## Why Marker Features Still Matter

It is tempting to dismiss an empty `Mixin` as ceremony. The markers earn their place through the concepts. Without a tag on the DSL, there is no `Mixin<Derived>` base, and therefore no way for `HasFeature` to return true. The marker is the surface that the concept queries.

Consider a generic combinator that should only accept DSLs participating in the result-based error-flow integration. With the `ResultFeature` marker in place, the combinator's signature is straightforward:

```cpp
template <typename D>
  requires dsl::HasFeature<D, dsl::ResultFeature>
auto
run_with_care (D &dsl, /* ... */)
{
  // body may assume the DSL opts into Result<T,E> flow
}
```

The body of the function need not call any injected method, because there is no injected method to call. The constraint is purely documentary at the type level — but it is enforced documentary, which is exactly what a static-typing toolkit should provide.

The same reasoning applies to `LazyFeature`, `Monadic`, `CombinatorParser`, and `TaskPipeline`. Each marker is a contract that the DSL author has elected to advertise, and each can be checked by user code through `HasFeature`. The later chapters devoted to those integrations (Chapters 19 through 26) assume that the corresponding marker has been listed where appropriate.

## Cross-References

The feature tags introduced here are the building blocks for the rest of the manual. Chapter 6 covers the `Pipeline` feature and the `operator|` machinery in full. Chapter 7 covers `Operators` and predicate composition. Chapter 8 covers `PatternMatch` together with `match`, `when`, and `otherwise`.

Chapters 10 through 12 cover the `ASTNode` value type and the `leaf`/`node` builders that `AST` wraps. Chapter 13 covers `Rewrite`, Chapter 14 covers rewrite sets and fixpoint optimization, and Chapter 15 covers `ExprTemplates` together with the `BinExpr` and `UnaryExpr` types.

Chapter 17 covers `CustomLiterals` in full, including `lit`, `literal_set`, and `parse_literal`. Chapter 18 covers `Memoization`. The marker features for the higher-level integrations are covered in Chapters 19 through 26: `LazyFeature` in Chapter 19, `Monadic` in Chapters 20 and 21, `ResultFeature` in Chapter 22, `CombinatorParser` in Chapters 23 through 25, and `TaskPipeline` in Chapter 26.

The `FixedString` non-type parameter used by `AST`'s `make_leaf` and `make_node` is documented in Chapter 4. The CRTP base `dsl::DSL<Derived, Features...>` itself is documented in Chapter 3.

## A Note on Naming Conventions

The library follows a consistent naming scheme for feature tags. Each tag is a single CamelCase noun phrase in the `dsl` namespace: `Pipeline`, `Operators`, `PatternMatch`, `AST`, `Rewrite`, `ExprTemplates`, `CustomLiterals`, `Memoization`, `LazyFeature`, `Monadic`, `ResultFeature`, `CombinatorParser`, `TaskPipeline`. The trailing `Feature` suffix is used selectively, typically when the bare noun would collide with a more prominent type (for example, `LazyFeature` avoids confusion with the `Lazy<T>` template of Chapter 19).

The mixin's injected members follow their own conventions. Static factories are verbs (`wrap`, `make_pred`, `make_leaf`, `make_node`, `parse_literal`). Instance methods that mutate derived state are also verbs (`clear_cache`, `rewrite`). Operators are `friend` functions, never members, so that they participate in argument-dependent lookup without bloating the derived class's member set.

Users adding their own feature tags are encouraged to follow the same conventions. The two-level `Tag` / `Mixin<Derived>` shape is mandatory; the naming of injected members is up to the author, but consistency with the library's verbs-and-factories style makes user features read as if they were part of the toolkit.

## Combining Markers and Method-Injecting Features

A realistic DSL frequently mixes markers with method-injecting features. A parser-DSL, for instance, might list `Pipeline` (for `wrap`), `AST` (for `make_node`), and `CombinatorParser` (as a marker advertising that it consumes parser combinators), all at once:

```cpp
struct ParserDSL
  : dsl::DSL<ParserDSL,
             dsl::Pipeline,
             dsl::AST,
             dsl::CombinatorParser>
{};
```

The resulting class has `wrap`, `make_leaf`, and `make_node` as callable members, and also satisfies `HasFeature<ParserDSL, dsl::CombinatorParser>`. Generic parser infrastructure can therefore gate itself on the marker while ignoring the injected factories, and generic tree-building infrastructure can do the reverse.

This separation of concerns is the practical payoff of the three-category taxonomy. Markers carry intent without carrying code; method-injecting features carry code without prescribing intent; operator-injecting features carry syntax for a specific subdomain. A DSL composes the three freely.

## Reviewing the Conditional Contract Once More

Because the conditional-member contract is central to two of the most useful features, it is worth restating the rules precisely.

For `Rewrite::rewrite` to exist on a DSL, the derived class must define a static member named `rules`. The member's type is expected to be a `RewriteSet` (Chapter 14), but the constraint checks only the name. The body of `rewrite` calls `Derived::rules.apply(tree)`, so the member must in practice support that call.

For `CustomLiterals::parse_literal` to exist on a DSL, the derived class must define a static member named `literals`. The member's type is expected to be a `LiteralSet` (Chapter 17), and the body of `parse_literal` calls `Derived::literals.parse(input)`.

In both cases, omitting the static member does not produce a compile error at the point where the DSL is defined. The error, if any, arises only when the gated method is called or when the corresponding concept (`Rewritable`, `HasLiterals`) is evaluated. This deferred diagnostic is a deliberate consequence of the `requires requires` idiom: the method is silently removed from the overload set when its precondition is unmet, rather than triggering a hard error in the mixin body.

The utility concepts exist partly to make that silent removal audible. A user who writes a generic function constrained with `Rewritable<D>` gets a clean diagnostic at the call site if `D` lacks `rules`, instead of a deep template error inside `rewrite`. Using the concepts is therefore the recommended way to interface with the conditional features from generic code.

## Putting It Together

A feature tag is, in summary, a small struct with a nested `Mixin` template. The tag is passed to `dsl::DSL`; the mixin is inherited. Pure markers contribute no members but enable concept-based introspection. Method-injecting features contribute static or instance member functions that typically forward to free functions in the `dsl` namespace. Operator-injecting features contribute `friend` operators found by argument-dependent lookup. Conditional members gated on `requires requires { Derived::member; }` let a feature demand a static member from the derived class without forcing a hard error at definition time.

The utility concepts — `HasFeature`, `Pipeable`, `ExprLike`, `HasLiterals`, `Rewritable` — sit on top of this machinery and let user-written templates query which features a DSL has mixed in. They are the primary interface between the feature-tag vocabulary and generic user code, and they are the reason the marker features exist at all.

The remainder of this manual examines each feature tag in detail, in the order suggested by the cross-references above. Readers who wish to write their own feature tags will find the idiom fully demystified by the illustrative `Greeter` and `VerboseGreeter` examples; everything else is variation on the same two-level shape.

## Summary

This chapter has catalogued the feature-tag idiom that underlies all of DSLtk's higher-level machinery. A feature tag is an empty or near-empty struct exposing a nested `template <typename Derived> struct Mixin`, and `dsl::DSL<Derived, Features...>` inherits each listed feature's `Mixin<Derived>` through variadic multiple inheritance.

Three categories of feature were identified. Pure markers (`PatternMatch`, `LazyFeature`, `Monadic`, `ResultFeature`, `CombinatorParser`, `TaskPipeline`) inject nothing but enable concept-based introspection. Method-injecting features (`Pipeline`, `Operators`, `AST`, `Memoization`, and the conditional `Rewrite` and `CustomLiterals`) contribute static or instance member functions. Operator-injecting features (`ExprTemplates`) contribute `friend` operators found through argument-dependent lookup.

The conditional-member contract, expressed through `requires requires { Derived::rules; }` and `requires requires { Derived::literals; }`, makes `rewrite` and `parse_literal` available only when the derived class supplies the corresponding static member. The utility concepts `HasFeature`, `Pipeable`, `ExprLike`, `HasLiterals`, and `Rewritable` mirror those contracts and provide the standard way to gate generic user code on the presence of a feature. With this vocabulary in hand, the remaining chapters examine each individual feature in turn.
