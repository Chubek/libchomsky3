# Chapter 13: The Rewrite Feature: Rules and Predicates

The previous three chapters built up the static side of the AST: Chapter 10 introduced the `ASTNode` value type, Chapter 11 showed how `leaf<>` and `node<>` construct trees, and Chapter 12 explained how to traverse and dump them. This chapter turns to the dynamic side. The Rewrite feature is the mechanism by which DSLtk transforms one AST into another, structurally equivalent but improved, by applying pattern-to-action rules. A rule fires when a predicate matches a node; the rule's action then produces a replacement node.

Rewriting is the workhorse of program transformation. Constant folding, algebraic simplification, desugaring, dead-code elimination, and peephole optimization are all, at their core, repeated application of small rewrite rules. DSLtk factors this into three pieces: a `RewriteRule` that pairs a predicate with a transformer, a `RewriteSet` that drives a collection of rules to a fixpoint (the subject of Chapter 14), and a `Rewrite` feature tag that grafts rule storage onto a DSL. This chapter covers the rule itself in detail; Chapter 14 covers sets and fixpoint iteration.

The design is deliberately functional. Rules do not mutate nodes in place. A transformer returns a brand-new `ASTNode`, leaving the input untouched. This makes rule application referentially transparent, easy to reason about, and trivially composable: the output of one rule can be fed straight into the next. The cost is allocation, which is acceptable for an embedded DSL toolkit that prioritizes clarity over raw throughput.

## What the Rewrite Feature Is For

An AST produced by a parser is rarely in the form a downstream consumer wants. The parser's job is to capture structure faithfully; the rewriter's job is to massage that structure into a canonical, simplified, or desugared form. Separating the two concerns keeps both stages simple. The parser does not need to know that `(+ x 0)` should become `x`; the rewriter does not need to know how the `+` was parsed.

Concretely, the Rewrite feature answers the question: given a tree, how does one declaratively express "wherever you see this shape, replace it with that shape"? The answer is a `RewriteRule`. Each rule carries a predicate that recognises a shape and a transformer that builds the replacement. Bundling many rules into a `RewriteSet` and iterating them to a fixpoint yields a full optimization pass.

The feature is opt-in. A DSL gains rewriting capabilities by listing `dsl::Rewrite` among its feature tags. The mixin that comes with the tag adds a thin `rewrite()` entry point that delegates to a static `rules` member defined by the derived DSL. Nothing about rewriting is hard-wired into the base `dsl::DSL` template; it is composed in through CRTP, exactly as described in Chapter 5 for every other feature.

## The RewriteRule Template

The heart of the feature is the `RewriteRule` primary template. It is parameterised by a compile-time name `Tag`, a predicate type `Pred`, and a transformer type `Trans`. The tag is a `FixedString` non-type template parameter (Chapter 4), used purely for identification, logging, and debugging. The predicate and transformer are stored by value.

The definition, quoted verbatim from the header, is:

```cpp
template <FixedString Tag, typename Pred, typename Trans> struct RewriteRule
{
  Pred pred;
  Trans trans;
  static constexpr std::string_view name = Tag.view ();

  constexpr RewriteRule (Pred p, Trans t)
      : pred (std::move (p)), trans (std::move (t))
  {
  }

  /**
   * @brief Attempts to apply this rule to a node.
   * @param n The node to test and potentially transform.
   * @return The transformed node if pred matched, or std::nullopt.
   */
  std::optional<ASTNode>
  try_apply (const ASTNode &n) const
  {
    if (pred (n))
      return trans (n);
    return std::nullopt;
  }
};
```

Three things deserve notice. First, the rule holds its callables as members, so a rule is a self-contained, copyable value with no external state. Second, the `name` member is a compile-time `string_view` derived from the tag, available for diagnostics. Third, the entire public surface of a rule is a single method, `try_apply`, which is the only way the rule ever interacts with a node.

The predicate is invoked first. If it returns `true`, the transformer is invoked on the same node and its result is returned wrapped in a `std::optional`. If the predicate returns `false`, the method returns `std::nullopt` and the node passes through unchanged. The rule itself never throws and never recurses; it simply tests and, on success, replaces.

## The Predicate Signature

A predicate is any callable that can be invoked with a `const ASTNode&` and returns something contextually convertible to `bool`. In practice this means a lambda, a function pointer, or a `dsl::Predicate` built with the Operators feature of Chapter 7. The predicate inspects the node through the `ASTNode` accessors introduced in Chapter 10: `tag()`, `is_leaf()`, `value()`, `children()`, `child(i)`, and `size()`.

Predicates should be total. They must not throw, must not depend on global mutable state, and must give a deterministic answer for every possible node. A predicate that throws will tear down the entire rewrite pass, because `try_apply` does not catch. A predicate that inspects out-of-range children via `child(i)` will likewise throw, since `child` is bounds-checked with `at()`. Defensive predicates guard every index access with a `size()` check first.

Consider a predicate that recognises the shape `(+ leaf leaf)`, a binary addition of two leaves. It must verify four things: the node is not a leaf, its tag is `"add"`, it has exactly two children, and both children are leaves. Writing these checks out is verbose but unambiguous:

```cpp
auto is_add_two_leaves = [](const dsl::ASTNode& n) {
  if (n.is_leaf()) return false;
  if (n.tag() != "add") return false;
  if (n.size() != 2) return false;
  return n.child(0).is_leaf() && n.child(1).is_leaf();
};
```

The same predicate can be expressed more compactly by short-circuiting, but the explicit form reads better in a manual and survives refactoring. Note how `is_leaf()` is checked before `tag()` is even examined; this is not strictly necessary because a leaf can carry any tag, but it documents the intent that the rule operates on inner nodes only.

Predicates may inspect the values of leaf children. The `value()` member returns the leaf's payload as a `const std::string&` (Chapter 10). A predicate that recognises a numeric leaf can therefore test `n.child(0).value()` against a parsed number, or simply check that the string consists of digits. This is the mechanism behind constant folding, where the rule must decide whether two leaves are foldable numbers.

## The Transformer Signature

The transformer is any callable that takes a `const ASTNode&` (the matched node) and returns an `ASTNode`. The returned node completely replaces the matched node in the tree. There is no in-place mutation; the matched node is discarded and the transformer's result takes its place. The transformer is free to reuse children of the matched node, build an entirely fresh subtree, or return a single leaf.

A transformer that performs constant folding for `(+ leaf leaf)` reads the two leaf values, converts them to integers, adds them, and builds a new leaf holding the sum. The builders from Chapter 11 make this concise:

```cpp
auto fold_add = [](const dsl::ASTNode& n) -> dsl::ASTNode {
  int a = std::stoi(n.child(0).value());
  int b = std::stoi(n.child(1).value());
  return dsl::leaf<"num">(a + b);
};
```

A transformer that performs algebraic simplification for `(+ x 0)` returns the non-zero operand unchanged. Because the predicate has already established the shape, the transformer can simply reach in and return the relevant child. Returning a child by value copies it; this is the intended semantics, since the child must outlive the discarded parent.

```cpp
auto add_zero = [](const dsl::ASTNode& n) -> dsl::ASTNode {
  if (n.child(1).value() == "0") return n.child(0);
  return n.child(1);
};
```

A transformer must always return a valid node. Returning a default-constructed `ASTNode` is legal but rarely what is intended: the default constructor produces an empty leaf with an empty tag, which will then be dumped as `()`. If a transformer cannot produce a sensible replacement, the predicate should have been stricter. The contract is that the predicate returns `true` only when the transformer can definitely produce a valid result.

## The rule() Factory

Constructing a `RewriteRule` directly requires spelling out three template parameters and two decayed callable types. The `dsl::rule<Tag>(pred, trans)` factory deduces everything except the tag. The tag must still be written explicitly because it is a non-type template parameter of type `FixedString`; the compiler cannot infer it from the arguments.

```cpp
template <FixedString Tag, typename Pred, typename Trans>
constexpr auto
rule (Pred &&p, Trans &&t)
{
  return RewriteRule<Tag, std::decay_t<Pred>, std::decay_t<Trans>>{
    std::forward<Pred> (p), std::forward<Pred> (t)
  };
}
```

Note the use of `std::decay_t` on the deduced callable types. This strips references and cv-qualifiers so that the rule stores the lambda by value, not by reference. Storing by reference would be a dangling-pointer hazard the moment the rule outlives the temporary lambda. The factory perfect-forwards the arguments into the stored members, so move-only callables are supported.

Usage is uniform across all rules. The tag is a compile-time string literal, the predicate is the first argument, and the transformer is the second:

```cpp
auto r = dsl::rule<"fold-add">(is_add_two_leaves, fold_add);
```

The tag `"fold-add"` becomes `r.name`, a `string_view` with static storage duration. It is available at compile time and can be embedded in diagnostics without any runtime cost. Tags need not be unique across a rule set, but duplicates make logs harder to read, so unique, descriptive tags are the convention.

## try_apply in Detail

The `try_apply` method is the only public entry point on a rule. Its contract is simple: test the predicate, and if it matches, return the transformed node wrapped in a `std::optional`; otherwise return `std::nullopt`. The method is `const`, signalling that rule application is observationally pure.

```cpp
auto maybe = r.try_apply(some_node);
if (maybe) {
  dsl::ASTNode replacement = std::move(*maybe);
  // ... splice replacement into the tree ...
}
```

The `std::optional` return type is what lets a rewriter distinguish "no match" from "match producing a node". A transformer cannot signal failure by returning a sentinel node; that would conflate legitimate sentinels with rule failure. Instead, failure is expressed at the type level by the empty optional. This is the same discipline used by `std::optional` throughout the standard library.

Because `try_apply` does not recurse, calling it on the root of a tree rewrites only the root. Children are untouched. A single call to `try_apply` is therefore a local, pointwise operation. Turning a pointwise rule into a tree-wide pass is the job of a driver, which Chapter 14 covers in the form of `RewriteSet::apply`. This chapter confines itself to single-rule, single-node application.

A rule may legitimately produce a replacement that itself would match the same rule again. For example, a desugaring rule that rewrites `(+ x x)` into `(* x 2)` produces a `*` node, which does not match the `+`-shaped predicate. But a rule that rewrites `(- (- x))` into `(+ x)` might, in combination with another rule, re-enter a matching shape. Whether to iterate is the driver's decision, not the rule's.

## Single-Rule Application by Hand

To make the locality concrete, consider applying one rule to a whole tree by hand, without the `RewriteSet` driver. The walk is bottom-up: rewrite the children first, then attempt the rule on the current node. This ordering ensures that simplifications deep in the tree are visible to the rule when it examines a parent.

```cpp
dsl::ASTNode apply_one(const dsl::RewriteRule<
                         "fold-add", decltype(is_add_two_leaves),
                         decltype(fold_add)> &r,
                       const dsl::ASTNode &n) {
  std::vector<dsl::ASTNode> kids;
  if (!n.is_leaf()) {
    kids.reserve(n.size());
    for (const auto &c : n.children()) kids.push_back(apply_one(r, c));
  }
  dsl::ASTNode cur = n.is_leaf() ? n
                                 : dsl::ASTNode{n.tag(), std::move(kids)};
  if (auto m = r.try_apply(cur)) return std::move(*m);
  return cur;
}
```

This skeleton is exactly what `RewriteSet::rewrite_once` does internally, minus the multi-rule loop. The key observation is that the recursion lives outside the rule. A rule is a leaf function; the rewriter is the recursive engine. Keeping the two separate means a rule author never has to think about traversal, only about the shape being matched.

The cost of this bottom-up walk is one copy of the tree per pass. If no rule fires anywhere, the walk still rebuilds every inner node. For trees of modest size this is fine; for very large trees, Chapter 14 discusses the iteration budget `max_iterations` that bounds the total work.

Note that the hand-rolled driver above is only illustrative. Production code should use `dsl::rewrite_set(...).apply(tree)`, which generalises the walk to any number of rules and adds a fixpoint loop. The point of the illustration is to expose what the driver does, so that rule authors understand the environment their rules run in.

## Building Predicates with the Operators Feature

Chapter 7's Operators feature supplies a `Predicate` type and a small algebra for composing predicates with `&&`, `||`, and `!`. Using `dsl::Predicate` instead of raw lambdas can make rule intent more declarative, especially when a predicate is a conjunction of many small tests.

```cpp
dsl::Predicate<dsl::ASTNode> is_inner =
    [](const dsl::ASTNode& n){ return !n.is_leaf(); };
dsl::Predicate<dsl::ASTNode> is_add =
    [](const dsl::ASTNode& n){ return n.tag() == "add"; };
dsl::Predicate<dsl::ASTNode> arity_two =
    [](const dsl::ASTNode& n){ return n.size() == 2; };

auto folded_pred = is_inner && is_add && arity_two;
```

The composed `folded_pred` is itself a callable usable as the first argument to `dsl::rule`. Whether to use the algebra or raw lambdas is a stylistic choice. The algebra shines when the same sub-predicates are shared across many rules; raw lambdas are clearer for one-off, tightly scoped tests.

Either way, the predicate ultimately reduces to a callable taking `const ASTNode&` and returning `bool`. The `RewriteRule` template is indifferent to the concrete type, because `Pred` is a deduced template parameter. This is the payoff of the generic design: any callable that satisfies the signature is admissible.

## Worked Example: Constant Folding

Constant folding replaces a computation on constants with the constant result. The classic case is `(+ 2 3) -> 5`. The rule needs a predicate that recognises a binary `add` whose two children are numeric leaves, and a transformer that adds them.

```cpp
auto is_numeric_leaf = [](const dsl::ASTNode& n) {
  if (!n.is_leaf()) return false;
  const auto& v = n.value();
  if (v.empty()) return false;
  for (char c : v) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
};

auto fold_const_add = [](const dsl::ASTNode& n) -> dsl::ASTNode {
  int a = std::stoi(n.child(0).value());
  int b = std::stoi(n.child(1).value());
  return dsl::leaf<"num">(a + b);
};

auto r_fold_add = dsl::rule<"fold-const-add">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="add" && n.size()==2
          && is_numeric_leaf(n.child(0)) && is_numeric_leaf(n.child(1));
    },
    fold_const_add);
```

Applied to `dsl::node<"add">(dsl::leaf<"num">("2"), dsl::leaf<"num">("3"))`, the rule yields `(num 5)`. Applied to `dsl::node<"add">(dsl::leaf<"num">("2"), dsl::leaf<"var">("x"))`, the predicate fails and the node is left alone. The rule is total: any node that is not a foldable addition is simply passed through.

The same pattern generalises to `mul`, `sub`, and `div`. Each operator gets its own rule, its own predicate, and its own arithmetic in the transformer. A constant-folding pass is then the union of these rules in a `RewriteSet`, iterated to fixpoint so that nested constant expressions collapse from the leaves upward.

## Worked Example: Algebraic Simplification

Algebraic identities such as `(+ x 0) -> x` and `(* x 1) -> x` are the bread and butter of expression simplifiers. The predicate must recognise the operator and the identity element, and the transformer returns the other operand. Because the identity can appear on either side, two rules per operator are typical, or one rule whose transformer checks both positions.

```cpp
auto add_zero = dsl::rule<"add-zero">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="add" && n.size()==2
          && ((n.child(0).is_leaf() && n.child(0).value()=="0")
           || (n.child(1).is_leaf() && n.child(1).value()=="0"));
    },
    [](const dsl::ASTNode& n) -> dsl::ASTNode {
      if (n.child(1).is_leaf() && n.child(1).value()=="0") return n.child(0);
      return n.child(1);
    });

auto mul_one = dsl::rule<"mul-one">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="mul" && n.size()==2
          && ((n.child(0).is_leaf() && n.child(0).value()=="1")
           || (n.child(1).is_leaf() && n.child(1).value()=="1"));
    },
    [](const dsl::ASTNode& n) -> dsl::ASTNode {
      if (n.child(1).is_leaf() && n.child(1).value()=="1") return n.child(0);
      return n.child(1);
    });
```

Two further identities round out a basic simplifier. Multiplication by zero annihilates the expression, and adding a number to itself is twice the number. The annihilation rule must be careful: `(* x 0)` becomes `0`, but only when the zero is a leaf; otherwise the rule could fire on shapes where the "zero" is a non-constant subexpression with side effects, which a pure AST has no notion of, but which downstream consumers may care about.

```cpp
auto mul_zero = dsl::rule<"mul-zero">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="mul" && n.size()==2
          && ((n.child(0).is_leaf() && n.child(0).value()=="0")
           || (n.child(1).is_leaf() && n.child(1).value()=="0"));
    },
    [](const dsl::ASTNode&) -> dsl::ASTNode {
      return dsl::leaf<"num">("0");
    });
```

These three rules compose without interference. The `RewriteSet` driver of Chapter 14 will try each rule in turn on every node, so a node that matches `mul-one` will not accidentally be rewritten by `add-zero`. When two rules could both match the same node, the set applies them in declaration order, and the first one that fires wins for that pass.

## Worked Example: Desugaring

Desugaring rewrites a construct in terms of a more primitive one. A common example is rewriting `(+ x x)` as `(* x 2)`, on the grounds that multiplication by two is cheaper or more canonical than addition of a duplicated operand. The predicate must check that both operands of `add` are structurally identical, and the transformer must build a `mul` node whose first child is one copy of the operand and whose second child is the literal `2`.

Structural equality of `ASTNode` is provided directly by `operator==`, defined on `ASTNode` (Chapter 10). Two nodes are equal when their tags, leaf-ness, values, and recursively their children all match. This makes the predicate a one-liner.

```cpp
auto desugar_double_add = dsl::rule<"desugar-add-self">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="add" && n.size()==2
          && n.child(0) == n.child(1);
    },
    [](const dsl::ASTNode& n) -> dsl::ASTNode {
      std::vector<dsl::ASTNode> kids{
          n.child(0),
          dsl::leaf<"num">("2")
      };
      return dsl::ASTNode{"mul", std::move(kids)};
    });
```

The transformer builds the `mul` node by hand, using the inner-node constructor `ASTNode(std::string_view tag, std::vector<ASTNode> children)` documented in Chapter 10. It could equally well have used `dsl::node<"mul">(n.child(0), dsl::leaf<"num">("2"))` from Chapter 11; the two are equivalent and the choice is stylistic.

Desugaring rules like this one are where the "replacement does not rebuild children" caveat bites. The transformer returns a `mul` whose first child is a copy of `n.child(0)`. That copy carries its own children, untouched. If the operand was itself a complex subexpression awaiting simplification, that simplification will happen in a later pass of the driver, not as part of this rule.

## Worked Example: Double-Negation Elimination

The bundled example `examples/10-rewrite-basics.cpp` demonstrates the canonical double-negation rule: `(- (- x)) -> x`. The predicate recognises a unary `neg` whose sole child is another `neg`, and the transformer reaches two levels down and returns the inner operand.

```cpp
auto rules = dsl::rewrite_set(
    dsl::rule<"double-neg">(
        [](const dsl::ASTNode& n) {
          return !n.is_leaf() && n.tag() == "neg" && n.size() == 1
              && !n.child(0).is_leaf() && n.child(0).tag() == "neg";
        },
        [](const dsl::ASTNode& n) { return n.child(0).child(0); }));

auto in = dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"num">(4)));
std::cout << rules.apply(in).dump() << "\n";
```

The predicate checks `n.size() == 1` before accessing `n.child(0)`, and checks that the child is itself an inner node before accessing its tag. This is the defensive style recommended earlier: every `child(i)` call is preceded by a `size()` or `is_leaf()` guard. The transformer then returns `n.child(0).child(0)`, the operand buried under the two negations.

Running the example prints `(num 4)`. The double negation has collapsed, leaving the bare numeric leaf. Notice that the rule is applied through `rewrite_set(...).apply(...)`, which provides the bottom-up recursion and fixpoint iteration. The rule itself contains no recursion; it only knows how to recognise and replace the two-deep `neg` shape.

This example also shows that a rule need not preserve the operator tag of the matched node. The input is a `neg` node; the output is a `num` leaf. The transformer is free to return any node whatsoever, including one of a completely different category. The only constraint is that the result be a valid `ASTNode`.

## Worked Example: A Dead-Code Elimination Stub

Dead-code elimination removes computations whose results are never observed. In a pure AST without dataflow, a full DCE pass is out of scope, but a useful stub can recognise a `seq` node whose first child is a pure constant and discard it. The predicate checks the shape; the transformer returns the second child.

```cpp
auto dce_const_seq = dsl::rule<"dce-const-seq">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="seq" && n.size()==2
          && n.child(0).is_leaf()
          && n.child(0).tag()=="num";
    },
    [](const dsl::ASTNode& n) -> dsl::ASTNode {
      return n.child(1);
    });
```

Applied to `dsl::node<"seq">(dsl::leaf<"num">("7"), dsl::leaf<"var">("x"))`, the rule yields `(var x)`. The constant `7` is gone. This is a stub because it makes no attempt to determine whether the discarded child has side effects; in a real compiler, purity analysis would feed into the predicate. The shape of the rule, however, is exactly right: predicate recognises a discardable pattern, transformer returns the surviving subtree.

Stubs like this are a productive way to grow an optimisation pipeline. Each stub is a rule with a conservative predicate; as the compiler gains analyses, the predicates are relaxed to fire in more cases. Because rules are independent values, relaxing one rule never affects the others, and the `RewriteSet` driver of Chapter 14 reaps the accumulated improvements automatically.

## The Rewrite Feature Tag

The `Rewrite` feature tag is the bridge between free-standing rule values and a CRTP-derived DSL. It is an empty struct with a nested `Mixin` template, exactly like the other feature tags described in Chapter 5. The mixin adds a single method, `rewrite`, to the derived DSL.

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

The method is constrained by a `requires` clause that checks for a static `rules` member on the derived type. This keeps the mixin SFINAE-friendly: a DSL that lists `dsl::Rewrite` but forgets to define `rules` will simply not have a usable `rewrite` method, rather than failing to instantiate. The error surfaces at the call site, where it is most actionable.

What the mixin provides, concretely, is three things. First, a `rewrite(const ASTNode&)` member that takes a tree by const reference and returns the rewritten tree by value. Second, a contract that the derived DSL defines a static `rules` member of a type that has an `apply(const ASTNode&)` method (typically `dsl::RewriteSet`, Chapter 14). Third, nothing else: the mixin adds no storage, no other methods, and no hidden hooks.

The mixin does not own the rules. The rules live in `Derived::rules`, a static member. This is important for two reasons. Static storage means the rules are constructed once, at program initialisation, and never copied per DSL instance. And because the rules are a static member of the derived type, different DSLs can carry different rule sets without interfering.

## Wiring Rewrite into a DSL

A DSL that wants rewriting declares `dsl::Rewrite` among its feature tags and supplies a static `rules` member. The result is a DSL instance on which `m.rewrite(tree)` is a one-liner.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::AST, dsl::Rewrite> {
  static inline auto rules = dsl::rewrite_set(
      dsl::rule<"add-zero">(/* pred */, /* trans */),
      dsl::rule<"mul-one">(/* pred */, /* trans */),
      dsl::rule<"fold-const-add">(/* pred */, /* trans */));
};

MyDSL m;
dsl::ASTNode optimized = m.rewrite(some_tree);
```

The `static inline auto rules` declaration deduces the type of the `RewriteSet` from the `rewrite_set` factory. Because the factory returns a `RewriteSet<...>` with one template parameter per rule, and each rule carries a unique predicate and transformer type, the full type of `rules` is long and unwieldy. The `auto` declaration hides it entirely.

The choice of `static inline` (rather than `static`) is deliberate. `static inline` guarantees that there is exactly one instance of `rules` across translation units, avoiding ODR problems if the DSL header is included in multiple TUs. It also permits the member to be initialised inline with a non-trivial expression, which a pre-C++17 `static` member could not.

Once wired in, `rewrite` is available on every instance of `MyDSL`. The method is `const`, so it can be called on a `const` DSL instance. It takes the input tree by const reference and returns the rewritten tree by value; the input is never modified.

## Worked Example: A Small Simplifier DSL

Putting the pieces together, here is a minimal DSL whose sole purpose is to simplify arithmetic trees. It bundles three rules into a `RewriteSet` and exposes them through the `Rewrite` feature tag.

```cpp
struct Simplifier : dsl::DSL<Simplifier, dsl::AST, dsl::Rewrite> {
  static inline auto rules = dsl::rewrite_set(
      dsl::rule<"add-zero">(
          [](const dsl::ASTNode& n){
            return !n.is_leaf() && n.tag()=="add" && n.size()==2
                && n.child(1).is_leaf() && n.child(1).value()=="0";
          },
          [](const dsl::ASTNode& n){ return n.child(0); }),
      dsl::rule<"mul-one">(
          [](const dsl::ASTNode& n){
            return !n.is_leaf() && n.tag()=="mul" && n.size()==2
                && n.child(1).is_leaf() && n.child(1).value()=="1";
          },
          [](const dsl::ASTNode& n){ return n.child(0); }),
      dsl::rule<"double-neg">(
          [](const dsl::ASTNode& n){
            return !n.is_leaf() && n.tag()=="neg" && n.size()==1
                && !n.child(0).is_leaf() && n.child(0).tag()=="neg";
          },
          [](const dsl::ASTNode& n){ return n.child(0).child(0); }));
};

Simplifier s;
auto t = dsl::node<"add">(
    dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"var">("x"))),
    dsl::leaf<"num">("0"));
std::cout << s.rewrite(t).dump() << "\n";  // => (var x)
```

The driver rewrites bottom-up, so the inner `(- (- x))` collapses to `x` first. The outer `(+ x 0)` then matches `add-zero` and collapses to `x`. The final output is `(var x)`. Two rules fired, in sequence, across two passes of the tree, even though the user wrote a single `s.rewrite(t)` call. This is the leverage the Rewrite feature provides: a declarative rule list plus an automatic driver.

The order of rules in the set matters when several rules could match the same node. In the example above, the rules are mutually exclusive, so order is irrelevant. In general, put more specific rules before more general ones, just as one would order cases in a pattern-match (Chapter 8).

## Predicates That Inspect Children

Many rules need to look beyond the immediate node. A predicate for `(+ x x)` desugaring compares the two children for equality. A predicate for constant folding inspects whether both children are numeric leaves. The `ASTNode` API of Chapter 10 makes these inspections straightforward, but each access carries a bounds-check obligation.

The safe pattern is: check `is_leaf()` first, then `tag()`, then `size()`, and only then `child(i)`. This ordering ensures that every `child(i)` call is preceded by a proof that `i` is in range. Reversing the order, or skipping the `size()` check, leads to predicates that throw on unexpected shapes, which terminates the entire rewrite pass.

When a predicate must inspect a grandchild, the same discipline applies recursively. The double-negation rule accesses `n.child(0).child(0)`, but only after verifying `n.size()==1`, `n.child(0)` is not a leaf, and `n.child(0).tag()=="neg"`. Each level of descent adds one more layer of guards. This is the price of a total, throwing-averse predicate.

Predicates that walk an arbitrary number of children, such as "any child is a constant", can use a loop over `n.children()`. The `children()` accessor returns a `const std::vector<ASTNode>&`, so range-for is natural. Such predicates are still total: an empty vector yields an empty loop, which is well-defined.

## Transformers That Rebuild Children

A transformer is not obligated to preserve the children of the matched node. It can drop them, reorder them, duplicate them, or wrap them in new structure. The `desugar-add-self` rule earlier kept one child and discarded the other; the `mul-zero` rule discarded both. The `dce-const-seq` rule kept the second child and dropped the first.

When a transformer does want to keep the children but change the parent's tag, it must rebuild the node, because `ASTNode` has no `set_tag` method. The inner-node constructor `ASTNode(std::string_view tag, std::vector<ASTNode> children)` is the tool for this. Copy the children into a new vector, construct a new node with the new tag, and return it.

```cpp
auto retag_add_to_mul = dsl::rule<"retag-add-mul">(
    [](const dsl::ASTNode& n){
      return !n.is_leaf() && n.tag()=="add" && n.size()==2;
    },
    [](const dsl::ASTNode& n) -> dsl::ASTNode {
      return dsl::ASTNode{"mul", std::vector<dsl::ASTNode>{n.child(0), n.child(1)}};
    });
```

This rule is artificial but illustrates the technique. A more realistic use is re-tagging a parsed `expr` node to a more specific category after a syntactic category has been refined. The children are preserved by copy; their own subtrees come along for the ride, again untouched.

A transformer that needs to rewrite children as part of building its result should not do so by hand. Instead, it should return a node whose children are the originals, and rely on the driver's bottom-up walk to have already rewritten those children before the rule fires. This is the key invariant of the bottom-up walk: when a rule's transformer sees a child, that child has already been fully rewritten for the current pass.

## Pitfalls: Totality, Validity, and Recursion

Three pitfalls recur in rule authoring. The first is predicate non-totality. A predicate that throws, whether by `child(i)` out of range or by calling a throwing function on a leaf value, will crash the rewrite pass. Predicates must inspect defensively and return `false` for any shape they do not recognise. The `is_leaf()` and `size()` guards are not optional ceremony; they are the rule's contract with the driver.

The second pitfall is an invalid transformer result. A transformer that returns a default-constructed `ASTNode` produces an empty-tag leaf, which dumps as `()`. This is rarely intended. If a transformer cannot build a sensible replacement, the predicate should not have matched. The contract is that the predicate's `true` implies the transformer's success. Rules that violate this contract produce garbage trees silently, which is worse than a crash because it is harder to spot.

The third pitfall is expecting a rule to recurse. It does not. `try_apply` tests the predicate on one node and, if it matches, replaces that one node. If the replacement itself contains subexpressions that match the rule, they will be rewritten only on the next pass of the driver, not by the rule itself. A rule that tries to recurse by calling itself from inside the transformer will loop forever or blow the stack; recursion is the driver's job, and the driver of Chapter 14 is built to do it safely with an iteration budget.

A fourth, subtler pitfall is worth naming: replacing a node does not rebuild its children. When a transformer returns `n.child(0)`, it returns a copy of that child with its subtree intact. The subtree is not re-rewritten by this rule application. If the subtree needs further rewriting, it will receive it in a subsequent pass, provided the driver is still iterating. A rule that depends on its own output being immediately re-examined must therefore be paired with a driver that iterates to fixpoint, which is exactly what `RewriteSet::apply` does.

## Summary

The Rewrite feature expresses tree-to-tree transformations as self-contained rules. A `RewriteRule<Tag, Pred, Trans>` pairs a predicate, which decides whether the rule applies to a given `ASTNode`, with a transformer, which produces the replacement `ASTNode`. The single public method `try_apply` tests the predicate and returns either the transformed node wrapped in a `std::optional` or `std::nullopt`. The `dsl::rule<Tag>(pred, trans)` factory deduces the callable types and stores them by value, leaving only the tag to be written explicitly.

Predicates inspect nodes through the Chapter 10 accessors `tag()`, `is_leaf()`, `value()`, `children()`, `child(i)`, and `size()`, and must be total: every `child(i)` call must be guarded by a `size()` check, and the predicate must return `false` for any shape it does not recognise. Transformers return a valid `ASTNode` by any means available, including the `leaf<>` and `node<>` builders of Chapter 11 and the raw `ASTNode` constructors. They may keep, drop, or rebuild children, but they do not recurse; recursion is the responsibility of the driver, covered in Chapter 14 as `RewriteSet`.

The `Rewrite` feature tag grafts rule application onto a CRTP-derived DSL through a mixin that adds a single `rewrite(const ASTNode&)` method, constrained to require a static `rules` member on the derived type. A DSL lists `dsl::Rewrite` among its feature tags, supplies `static inline auto rules = dsl::rewrite_set(...)`, and gains `m.rewrite(tree)` as its entry point. Worked examples in this chapter covered constant folding, algebraic simplification, desugaring, double-negation elimination, and a dead-code elimination stub, each illustrating the predicate-transformer discipline that underpins all rule authoring in DSLtk.
