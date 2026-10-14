# Chapter 14: Rewrite Sets and Fixpoint Optimization

Chapter 13 introduced the individual `RewriteRule`: a named pair of a predicate and a transformer, packaged through the `dsl::rule<Tag>(pred, trans)` factory. A single rule in isolation is useful for targeted peephole transformations, but real simplifiers and normalizers rarely consist of one rule. They are ensembles: a handful of identities, a constant folder, a dead-branch eliminator, all cooperating. This chapter presents the construct that bundles them, `RewriteSet`, and the fixpoint loop that drives them to convergence.

The chapter sits between the rule mechanics of Chapter 13 and the larger DSL feature surface. It assumes familiarity with the `ASTNode` value type from Chapter 10, the `leaf<>` and `node<>` builders from Chapter 11, the traversal idioms of Chapter 12, and the `RewriteRule` API from Chapter 13. The `Rewrite` feature tag defined here is consumed by DSL hosts exactly like the pipeline and operators features of Chapters 6 and 7.

## 14.1 Motivation: From Single Rules to Converging Passes

Consider the arithmetic tree `(* (+ x 0) 1)`. A rule that strips `(+ _ 0)` into `_` rewrites the inner node to `x`, yielding `(* x 1)`. But the outer multiplication by one is still there. A second rule, `(* _ 1) -> _`, must then fire to reach `x`. No single rule can do both jobs, and the second match only becomes available after the first has run.

A naive driver that walks the tree once, applying each rule at most once per node, would leave `(* x 1)` untouched: the `mul-one` rule was not applicable to the original `(* (+ x 0) 1)` because the right child was `1` but the left child was an unevaluated `add`, and the rule's intent is to simplify regardless. The driver must therefore re-examine nodes after rewriting them. Two natural strategies exist: re-run the entire pass until nothing changes, or carefully schedule re-visits. DSLtk chooses the former, because it is simple, correct for a broad class of rule sets, and matches the semantics of term rewriting systems.

This is the fixpoint model. A bundle of rules is applied repeatedly to the whole tree; each pass may rewrite nodes anywhere; the process stops when a complete pass produces a tree identical to the one it started from. At that point no rule fires anywhere, and the tree is a normal form with respect to the rule set. The `RewriteSet` template is the embodiment of this model.

## 14.2 The RewriteSet Template

`RewriteSet` is a variadic class template parameterized on the rule types it contains. Its definition opens with the documentation comment reproduced below, which captures its intent concisely.

```cpp
/**
 * @brief A set of rewrite rules applied repeatedly until fixpoint.
 *
 * @tparam Rules  Variadic list of RewriteRule types.
 *
 * Example:
 *   auto rules = dsl::rewrite_set(
 *       dsl::rule<"r1">(pred1, trans1),
 *       dsl::rule<"r2">(pred2, trans2)
 *   );
 *   auto optimized = rules.apply(tree);
 */
template <typename... Rules> struct RewriteSet
{
  std::tuple<Rules...> rules;
  std::size_t max_iterations = 100;

  explicit constexpr RewriteSet (Rules... rs) : rules (std::move (rs)...) {}
  // ... apply() and rewrite_once() ...
};
```

The set stores its rules by value in a `std::tuple<Rules...>`. Because `dsl::rule<Tag>(pred, trans)` returns a distinct type per rule (the tag is a `FixedString` non-type template parameter, and the predicate and transformer types are unique lambda types), each `RewriteSet` instantiation has a precise type that encodes the full list of rules it contains. Two sets that differ only in rule order have different types. This is deliberate: the type carries the entire compilation unit's worth of rule information, which permits inlining and removes the need for type erasure.

The `max_iterations` member caps the fixpoint loop at one hundred passes by default. This is a safety net against non-terminating rule sets, discussed later in the chapter. It is a public, non-`const` member, so a user who needs a larger or smaller cap may set it directly after construction. The cap is on passes, not on individual rule firings; a single pass already fires rules at every node of the tree.

Construction takes the rules by value. The `rewrite_set` factory forwards its arguments with perfect forwarding and decays their types, so the resulting `RewriteSet` holds fully-formed `RewriteRule` objects rather than references. The factory is the canonical way to build a set; direct construction is possible but verbose.

## 14.3 The Fixpoint Driver: apply()

The `apply` member function is the entry point that runs the fixpoint loop. Its definition is short and worth quoting verbatim.

```cpp
ASTNode
apply (ASTNode root) const
{
  for (std::size_t i = 0; i < max_iterations; ++i)
    {
      auto [changed, result] = rewrite_once (root);
      if (!changed)
        return result;
      root = std::move (result);
    }
  return root;
}
```

The loop is a classic fixpoint iteration. Each iteration calls `rewrite_once` to perform one bottom-up pass over the tree. The pass returns a pair: a boolean indicating whether any rule fired, and the possibly-rewritten tree. If no rule fired, the tree is stable and is returned immediately. Otherwise the rewritten tree becomes the input to the next iteration.

The loop is bounded by `max_iterations`. If convergence has not occurred by the cap, the most recent tree is returned. This means a non-terminating rule set does not hang the program; it merely produces a tree that is not fully normalized. The cap is a pragmatic compromise. For terminating rule sets, which is the common case for simplification, the loop exits via the `!changed` path long before the cap is reached.

Note that the loop re-runs the entire pass on the whole tree, not just on the subtrees that changed. This is wasteful in principle — a node that is already in normal form is re-examined on every pass — but it is simple and avoids the bookkeeping of dirty marking. For the tree sizes typical of embedded DSL work, the cost is acceptable. Users with very large trees or expensive predicates may wish to reduce `max_iterations` or restructure their rules to converge faster.

The `apply` function takes its argument by value. This is deliberate: it allows the implementation to move the tree around without aliasing concerns, and it makes the call site simple. Callers that hold a tree they wish to preserve can pass a copy; callers that do not can `std::move` into the call.

## 14.4 The Bottom-Up Pass: rewrite_once()

The single pass is where the actual rewriting happens. `rewrite_once` performs a bottom-up traversal: it rewrites the children of a node first, then attempts to apply the rules to the node itself. The full definition is reproduced below.

```cpp
std::pair<bool, ASTNode>
rewrite_once (const ASTNode &n) const
{
  bool any_changed = false;

  // Recursively rewrite children
  std::vector<ASTNode> new_children;
  if (!n.is_leaf ())
    {
      new_children.reserve (n.children ().size ());
      for (auto &child : n.children ())
        {
          auto [changed, rewritten] = rewrite_once (child);
          any_changed |= changed;
          new_children.push_back (std::move (rewritten));
        }
    }

  ASTNode current
      = n.is_leaf () ? n : ASTNode{ n.tag (), std::move (new_children) };

  // Try each rule on the current node
  std::apply (
      [&] (const auto &...r)
        {
          (
              [&]
                {
                  if (auto result = r.try_apply (current))
                    {
                      current = std::move (*result);
                      any_changed = true;
                    }
                }(),
              ...);
        },
      rules);

  return { any_changed, current };
}
```

The traversal is post-order. For a non-leaf node, each child is first processed recursively, producing a vector of rewritten children. A fresh node is then constructed from the original tag and the new children. This reconstruction is necessary because `ASTNode` is an immutable value type, as described in Chapter 10; there is no in-place mutation of children. The fresh node carries the same tag as the original but with rewritten children.

After the node is rebuilt, the rules are tried. The `std::apply` call unpacks the rule tuple and the fold expression invokes each rule's `try_apply` in declaration order. `try_apply`, defined in Chapter 13, returns `std::optional<ASTNode>`: a engaged optional when the predicate matched, disengaged otherwise. When a rule fires, `current` is replaced by the rule's output and `any_changed` is set. Crucially, the next rule in the fold sees the new `current`, not the original node. Rules therefore cascade within a single node visit.

Leaf nodes skip the child loop and go straight to rule application. This is correct because leaves have no children to rewrite, and rules may still fire on them — for instance, a rule that normalizes a numeric literal's textual representation.

## 14.5 Rule Ordering and Cascading Within a Node

Because the fold expression applies rules in declaration order and each firing rule replaces `current`, the order of rules in a `RewriteSet` is semantically meaningful. An earlier rule can transform a node into a shape that a later rule then matches. This is not first-match-wins; it is ordered, cascading application.

Consider a node `(+ x x)` and two rules: `add-self` rewrites it to `(* x 2)`, and `mul-one-left` rewrites `(* 1 _)` to `_`. If `add-self` is declared first, the node becomes `(* x 2)`; `mul-one-left` examines this result, finds it does not match, and leaves it. If the order were reversed, `mul-one-left` would run first on `(+ x x)`, fail to match, and then `add-self` would fire. The final result is the same in this case, but the cascade path differs.

The difference becomes observable when a later rule depends on an earlier rule's output. Suppose a third rule, `double-to-shift`, rewrites `(* x 2)` to `(<< x 1)`. Declared after `add-self`, it fires in the same pass, collapsing `(+ x x)` all the way to `(<< x 1)` in one visit. Declared before `add-self`, it never sees `(* x 2)` and the shift form is only produced on the next fixpoint iteration, if at all. Ordering is therefore a tool for guiding the cascade.

When two rules could both match the same node and their effects conflict, the earlier rule wins in the sense that it transforms the node first, and the later rule sees the transformed node. If the transformation removes the condition the later rule was waiting for, the later rule silently does nothing. There is no warning, no conflict resolution; the order is the resolution.

## 14.6 Bottom-Up Evaluation Order

The choice of bottom-up traversal — children before parent — is significant. It means that by the time a rule examines a node, all of its children have already been fully rewritten for the current pass. A rule can therefore rely on the children being in a pass-local normal form.

For constant folding this is essential. The rule `(+ num num) -> num` must see both children as numeric literals before it can compute their sum. If the children are themselves `add` nodes, bottom-up rewriting collapses them to literals first, so that by the time the parent is examined, its children are the folded literals. A top-down pass would require multiple fixpoint iterations to achieve the same effect for deeply nested expressions.

Bottom-up evaluation also means that a rule which deletes a subtree — for example, a dead-code eliminator that removes a side-effect-free branch — does so before the parent is rebuilt. The parent is constructed from the already-rewritten children, so a deleted child simply does not appear in the new child vector. This makes removal rules straightforward to write: the transformer returns a node with fewer children, and the parent rebuild picks up the new shape on the next visit, or immediately if the removal happened at the node itself.

The trade-off is that a rule cannot easily inspect the original, unrewritten children of a node. If a rule needs to match on a pattern that only exists before child rewriting, it must be split or reordered. In practice this is rarely needed, because most simplification rules are local and compose well under bottom-up evaluation.

## 14.7 The rewrite_set Factory

The `rewrite_set` factory is the conventional way to construct a `RewriteSet`. Its definition is minimal.

```cpp
template <typename... Rules>
constexpr auto
rewrite_set (Rules &&...rs)
{
  return RewriteSet<std::decay_t<Rules>...>{ std::forward<Rules> (rs)... };
}
```

The factory deduces the rule types from its arguments, decays them so that the set holds values rather than references, and forwards the arguments to the `RewriteSet` constructor. Because the function is `constexpr`, a rewrite set can in principle be constructed at compile time, although the `apply` member is not `constexpr` and the actual rewriting happens at runtime.

The factory returns by value, relying on copy elision. The returned object is a `RewriteSet` with a concrete, fully-spelled type. This type can be stored in a variable declared with `auto`, which is the usual idiom, or it can be used as a static member of a DSL host, as shown later in the chapter.

Because each rule's type involves unique lambda types, the type of the returned `RewriteSet` is unwieldy to spell by hand. The `auto` idiom is not just convenient; it is practically required. This is a common pattern in modern C++ template-heavy libraries and is consistent with the rest of DSLtk's API surface.

## 14.8 A First Complete Example

The following program builds a small rewrite set with two rules and applies it to a nested tree. It is the canonical "hello world" of rewrite sets.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto rules = dsl::rewrite_set(
      dsl::rule<"add-zero-right">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "add" && n.size() == 2
                && n.child(1).tag() == "num"
                && n.child(1).value() == "0";
          },
          [](const dsl::ASTNode& n) { return n.child(0); }),
      dsl::rule<"mul-one-right">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "mul" && n.size() == 2
                && n.child(1).tag() == "num"
                && n.child(1).value() == "1";
          },
          [](const dsl::ASTNode& n) { return n.child(0); }));
  auto in = dsl::node<"mul">(
      dsl::node<"add">(dsl::leaf<"num">(7), dsl::leaf<"num">(0)),
      dsl::leaf<"num">(1));
  std::cout << rules.apply(in).dump() << "\n";
}
```

The input tree is `(* (+ 7 0) 1)`. The first pass descends to the `add` node, where `add-zero-right` fires and replaces `(+ 7 0)` with `7`. The pass then rebuilds the parent `mul` node with the new left child `7`, giving `(* 7 1)`, where `mul-one-right` fires and replaces the whole node with `7`. Because both firings occur within a single `rewrite_once` pass, `any_changed` is true, so a second pass is attempted. The second pass finds no rule to fire on the leaf `7` and returns `changed == false`. The fixpoint loop exits, and `7` is printed.

This example demonstrates the bottom-up cascade: rewriting a child enabled a rule on the parent within the same pass. Without bottom-up evaluation, two passes would have been needed even for this small tree.

## 14.9 A Larger Simplifier: Multiple Rules Together

Real simplifiers combine several categories of rule. The set below folds constants, strips algebraic identities, and collapses self-addition. It is a single `RewriteSet`, applied as one pass.

```cpp
auto simplifier = dsl::rewrite_set(
    dsl::rule<"const-fold-add">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "add" && n.size() == 2
              && n.child(0).tag() == "num" && n.child(1).tag() == "num";
        },
        [](const dsl::ASTNode& n) {
          int a = std::stoi(n.child(0).value());
          int b = std::stoi(n.child(1).value());
          return dsl::leaf<"num">(std::to_string(a + b));
        }),
    dsl::rule<"add-zero">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "add" && n.size() == 2
              && n.child(1).tag() == "num" && n.child(1).value() == "0";
        },
        [](const dsl::ASTNode& n) { return n.child(0); }),
    dsl::rule<"mul-one">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "mul" && n.size() == 2
              && n.child(1).tag() == "num" && n.child(1).value() == "1";
        },
        [](const dsl::ASTNode& n) { return n.child(0); }),
    dsl::rule<"add-self">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "add" && n.size() == 2
              && n.child(0).dump() == n.child(1).dump();
        },
        [](const dsl::ASTNode& n) {
          return dsl::node<"mul">(n.child(0), dsl::leaf<"num">("2"));
        }));
```

Each rule is independent and narrow. None of them knows about the others; the set's behavior emerges from their composition. The constant folder handles `(+ 2 3)` but not `(+ x 0)`; the identity rules handle the latter but not the former; the self-addition rule handles `(+ x x)` but neither of the others. Together they cover a useful fragment of arithmetic simplification.

Applying this set to `(+ (+ 2 3) (+ x x))` proceeds as follows. The pass descends to the leftmost `(+ 2 3)`, where `const-fold-add` fires and produces `5`. It then visits `(+ x x)`, where `add-self` fires and produces `(* x 2)`. The parent `add` is rebuilt as `(+ 5 (* x 2))`. No rule matches this parent in the current pass, so the pass completes with `changed == true`. The next pass examines `(+ 5 (* x 2))`; no rule fires anywhere, so the loop exits. The result is `(+ 5 (* x 2))`, a normal form for this rule set.

## 14.10 Multi-Pass Convergence on Deeply Nested Trees

Consider `(+ (+ (+ 0 0) 0) x)`. Each `add-zero` rule fires on one level per pass, because the rule strips only the right child. The bottom-up pass rewrites the innermost `(+ 0 0)` to `0`, producing `(+ (+ 0 0) x)`. The parent `(+ 0 0)` — formerly `(+ (+ 0 0) 0)` — is now `(+ 0 0)` after its left child was rewritten, and `add-zero` fires on it too within the same pass, producing `(+ 0 x)`. Wait: the rule strips the right child when it is `0`. In `(+ (+ 0 0) 0)`, after the inner rewrite the node is `(+ 0 0)`, the right child is `0`, so the rule fires and the node becomes `0`. The outermost `(+ _ x)` then has left child `0`, so `add-zero` does not directly fire because it requires the right child to be `0`. The result after one pass is `(+ 0 x)`, which does not match `add-zero` (right child is `x`, not `0`). A second pass changes nothing. The tree is `(+ 0 x)`, which the set does not simplify further because there is no left-zero rule.

This illustrates an important point: the fixpoint converges to a normal form with respect to the given rules, not to a mathematically minimal form. If the user wants `(+ 0 x)` to become `x`, a separate `add-zero-left` rule must be added. The set is exactly as powerful as its rules; the fixpoint loop only ensures that the rules are applied exhaustively.

Deeper nesting that does converge in multiple passes arises when a rule's output creates a match for the same rule higher up. The double-negation eliminator from Chapter 13 is the canonical example.

```cpp
auto rules = dsl::rewrite_set(
    dsl::rule<"double-neg">(
        [](const dsl::ASTNode& n) {
          return !n.is_leaf() && n.tag() == "neg"
              && n.size() == 1 && !n.child(0).is_leaf()
              && n.child(0).tag() == "neg";
        },
        [](const dsl::ASTNode& n) { return n.child(0).child(0); }));
auto in = dsl::node<"neg">(dsl::node<"neg">(
    dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"num">(4)))));
std::cout << rules.apply(in).dump() << "\n";
```

The input is `(neg (neg (neg (neg 4))))`, four nested negations. One pass rewrites the inner pair `(neg (neg 4))` to `4`, leaving `(neg (neg 4))` at the outer level, which the same pass then rewrites to `4` because the cascade climbs as children are rewritten. In fact the bottom-up traversal collapses the entire chain in a single pass. If the rule were instead structured to remove only one pair per node visit, multiple passes would be required. The number of passes depends on the rule's structure, not only on the tree's depth.

## 14.11 Composing a Full Optimization Pass

A practical compiler pass often combines several kinds of rule into one `RewriteSet`. The categories below are illustrative; the exact rules depend on the target language.

```cpp
auto opt_pass = dsl::rewrite_set(
    // Constant folding
    dsl::rule<"fold-add">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "add" && n.child(0).tag() == "num"
              && n.child(1).tag() == "num";
        },
        [](const dsl::ASTNode& n) {
          int a = std::stoi(n.child(0).value());
          int b = std::stoi(n.child(1).value());
          return dsl::leaf<"num">(std::to_string(a + b));
        }),
    // Algebraic identities
    dsl::rule<"add-zero">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "add" && n.child(1).tag() == "num"
              && n.child(1).value() == "0";
        },
        [](const dsl::ASTNode& n) { return n.child(0); }),
    // Dead branch removal
    dsl::rule<"dead-branch">(
        [](const dsl::ASTNode& n) {
          return n.tag() == "if" && n.child(0).tag() == "num";
        },
        [](const dsl::ASTNode& n) {
          return n.child(0).value() == "0" ? n.child(2) : n.child(1);
        }));
```

The three categories cooperate. Constant folding produces numeric literals that the dead-branch rule can then test; the algebraic identities clean up the residual arithmetic. Because they share a single `RewriteSet`, they run in the same fixpoint loop and can trigger one another within a single pass or across passes.

This composition is the primary value proposition of `RewriteSet`. Writing each category as a separate pass would require chaining `apply` calls by hand and would re-traverse the tree redundantly. Bundling them into one set lets the bottom-up pass fire whichever rule matches at whichever node, in the order dictated by the tree's structure, with the fixpoint loop guaranteeing exhaustion.

## 14.12 The Rewrite Feature Tag

DSLtk exposes the rewrite capability to a DSL host through the `Rewrite` feature tag, following the mixin pattern introduced in Chapter 5. The tag is a struct containing a `Mixin` template intended for the CRTP base.

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

The mixin provides a single member function, `rewrite`, which delegates to `Derived::rules.apply`. The `requires requires { Derived::rules; }` clause constrains the function so that it is only available when the derived DSL declares a static `rules` member. This is a use of the `requires` expression form of concepts to detect the presence of a member by name, a lightweight form of structural contract.

The DSL host inherits the mixin by listing `dsl::Rewrite` among its feature tags, as described in Chapter 3 and Chapter 5. The host is then obligated to define a static `rules` member of `RewriteSet` type. If it does not, the `rewrite` method is silently absent rather than producing a hard error at the point of declaration; the constraint surfaces the error only at the call site, which is the conventional behavior for constrained members.

The delegation through `Derived::rules` means the rule set is shared across all instances of the DSL host. Because `RewriteSet` is a value type with no mutable state used during `apply`, sharing a single static instance across calls is safe and avoids reconstructing the set per call. The set is typically defined `static inline`, as the next section shows.

## 14.13 Integrating Rewrite into a DSL Host

The canonical pattern for a DSL host that exposes rewriting is shown below. The host declares its rule set as a static member and lists `dsl::Rewrite` among its features.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::AST, dsl::Rewrite> {
  static inline auto rules = dsl::rewrite_set(
      dsl::rule<"add-zero">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "add" && n.child(1).tag() == "num"
                && n.child(1).value() == "0";
          },
          [](const dsl::ASTNode& n) { return n.child(0); }));
};

int main() {
  MyDSL dsl;
  auto tree = dsl::node<"add">(dsl::leaf<"num">(42), dsl::leaf<"num">(0));
  auto simplified = dsl.rewrite(tree);
  std::cout << simplified.dump() << "\n";
}
```

The `dsl::AST` base provides the tree-building members documented in Chapter 11, and `dsl::Rewrite` provides the `rewrite` member. The two features compose without interference because they affect different parts of the host's interface. Additional features, such as `dsl::Pipeline` from Chapter 6 or `dsl::Operators` from Chapter 7, can be added in the same parameter list.

The `static inline auto rules` declaration initializes the set at namespace scope, so it is constructed once and shared. Because the rule objects hold only their predicates and transformers, which are stateless lambdas, the set is effectively a singleton with no per-call allocation. The `apply` member allocates only the rewritten tree nodes, which is inherent to the immutable `ASTNode` design.

Calling `dsl.rewrite(tree)` returns a new `ASTNode`; the original is untouched. This value semantics matches the rest of DSLtk's API and makes rewriting safe to compose with other tree-producing operations. The result of one `rewrite` call can be fed directly into another pass, into a matcher from Chapter 8, or into a traversal from Chapter 12.

## 14.14 Termination: When Fixpoints Reach Equilibrium

The fixpoint loop terminates when a pass produces no change, or when `max_iterations` is reached. For most simplification rule sets, the first condition is met quickly. The reason is structural: each simplification rule either shrinks the tree, replacing a compound node with a smaller one, or normalizes it into a canonical shape that the same rule will not match again. A tree cannot shrink indefinitely — it has a finite number of nodes — and a normalization step that does not shrink the tree typically moves it toward a form that subsequent rules will shrink.

The constant folder, for instance, replaces a three-node `(+ 2 3)` with a single leaf `5`, a net reduction of two nodes. The identity rules replace a two-child node with one of its children, a reduction of one node plus a subtree. The double-negation rule removes two `neg` wrappers at once. Each firing strictly decreases the tree size, so the total number of firings across all passes is bounded by the initial node count, and convergence is guaranteed.

Normalization rules that do not shrink the tree are subtler. A rule that rewrites `(+ a b)` into `(+ b a)` to enforce a canonical operand order does not change the node count. If a companion rule then simplifies the reordered node, termination still holds. But if two such commutativity rules disagree on the canonical order — one sorting ascending, one descending — the set can oscillate. This is the user's responsibility, discussed in the next section.

The `max_iterations` cap is a backstop. For a terminating set, the cap is never reached; for a non-terminating set, the cap bounds the damage. A user who suspects a set may not terminate can set `max_iterations` to a small value, such as ten, and inspect the output. If the output is still changing at the cap, the rules likely cycle.

## 14.15 Non-Terminating Rule Sets and How to Avoid Them

A rule set fails to terminate when the rules can move the tree through a cycle without ever reaching a stable form. The simplest example is a pair of rules `a -> b` and `b -> a`. Each pass fires one rule, producing the other's trigger, and the next pass fires the other rule, restoring the original. The tree oscillates forever between two shapes, and the cap is the only thing that stops the loop.

Cycles can be indirect. A rule that rewrites `(+ a b)` to `(+ b a)` and another that rewrites `(+ b a)` to `(+ a b)` — perhaps written with different predicate shapes that both match the commuted form — produce the same oscillation. The rules need not be obviously symmetric; they need only admit a cycle in the rewrite graph.

The remedy is to ensure that the rule set is oriented: every rule either strictly decreases some well-founded measure, or moves the tree toward a canonical form that no rule will move away from. The strict-decrease approach is the safest. If every rule reduces the node count, or reduces a lexicographic measure such as (node count, then a complexity score), the set terminates by the well-foundedness of that measure.

When a normalization rule that preserves size is unavoidable, it should be paired with a unique canonical target. For example, if a rule sorts the children of a commutative operator into a defined order, no other rule should disturb that order. The sorting rule then fires at most once per node per pass, and the canonical form is stable. Conflicting canonical forms are the most common cause of silent non-termination.

Finally, rules that grow the tree require special care. A rule like `x -> (+ x 0)`, which expands rather than simplifies, can be useful for normalization into a target language, but it must be paired with a rule that consumes the expanded form, and the pairing must be acyclic. Without an acyclic consumer, the expansion rule will fire on its own output forever. The `max_iterations` cap catches this, but the resulting tree is not the intended normal form.

## 14.16 Pitfalls in Practice

Several pitfalls await the unwary user of `RewriteSet`. The first is the interaction between rule ordering and fixpoint convergence. Because rules cascade within a node, an early rule can prevent a later rule from ever firing, by transforming the node into a shape the later rule does not match. If the later rule was intended to produce the canonical form, the result is a tree that converges to a non-canonical form. Reordering the rules, or splitting the later rule to match the transformed shape, resolves this.

The second pitfall is deep recursion cost. The `rewrite_once` pass is recursive, descending into children at each level. For a tree of depth `d`, the recursion depth is `d`. Trees produced by parsing deeply nested input, or by repeated expansion, can exceed comfortable stack limits. DSLtk does not impose a depth limit; the user must ensure that the trees fed to `apply` are of reasonable depth, or restructure the input. The bottom-up traversal also rebuilds every node on every pass, so the per-pass cost is linear in the tree size, and the total cost is linear in the size times the number of passes to convergence.

The third pitfall is rules that grow the tree, mentioned above. Beyond the termination concern, growing rules inflate the per-pass cost, because each pass must traverse the larger tree. A rule that doubles the tree size on each pass, run to a cap of one hundred, produces an exponentially large output. Growth rules should be used sparingly and always paired with a consumer.

The fourth pitfall is predicate cost. The fixpoint loop calls each predicate at each node on each pass. A predicate that itself traverses the subtree, rather than examining only the immediate node, multiplies the cost by the subtree size and can turn a linear pass into a quadratic one. Efficient predicates examine the node's tag and immediate children only, delegating deeper inspection to the bottom-up traversal that has already rewritten those children.

The fifth pitfall is relying on child identity across rewriting. Because children are rebuilt on each pass, a rule that captures a child by reference and stores it for later comparison will see stale data. Rules should be pure functions of the node passed to them; they should not retain state between calls. The `RewriteRule` design encourages this by taking the predicate and transformer as values and invoking them afresh on each node.

## 14.17 Tuning max_iterations

The default cap of one hundred passes is generous for typical simplification sets, which converge in a handful of passes. Lowering the cap can be useful when the set is known to converge quickly and the user wants a tighter bound on runaway behavior. Setting `max_iterations` to, say, five, turns the rewrite set into a bounded-iteration transformer that gives up early rather than running to exhaustion.

Raising the cap is rarely necessary, because a set that has not converged in one hundred passes is almost certainly cycling. The cap is a diagnostic tool as much as a safety one: if lowering it changes the output, the set is not converging and should be examined.

The member is set directly on the `RewriteSet` instance after construction. Because the set is typically a static member of a DSL host, the cap is set at definition time.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::AST, dsl::Rewrite> {
  static inline auto rules = [] {
    auto rs = dsl::rewrite_set(
        dsl::rule<"add-zero">(
            [](const dsl::ASTNode& n) {
              return n.tag() == "add" && n.child(1).tag() == "num"
                  && n.child(1).value() == "0";
            },
            [](const dsl::ASTNode& n) { return n.child(0); }));
    rs.max_iterations = 16;
    return rs;
  }();
};
```

The immediately-invoked lambda initializes the set, adjusts the cap, and returns the configured object. The resulting `rules` member has the desired cap. This pattern works because `RewriteSet` is a value type with a public, mutable `max_iterations` member.

## 14.18 Composing Rewrite Sets

Two `RewriteSet` instances cannot be directly concatenated, because their types differ and there is no merge operation. Composition is achieved by listing all rules in a single `rewrite_set` call. When rules are shared across several DSL hosts, the usual technique is to define the rules as free functions or static lambdas and assemble them into a set at each host.

```cpp
inline auto add_zero_rule() {
  return dsl::rule<"add-zero">(
      [](const dsl::ASTNode& n) {
        return n.tag() == "add" && n.child(1).tag() == "num"
            && n.child(1).value() == "0";
      },
      [](const dsl::ASTNode& n) { return n.child(0); });
}

inline auto mul_one_rule() {
  return dsl::rule<"mul-one">(
      [](const dsl::ASTNode& n) {
        return n.tag() == "mul" && n.child(1).tag() == "num"
            && n.child(1).value() == "1";
      },
      [](const dsl::ASTNode& n) { return n.child(0); });
}

auto simplifier = dsl::rewrite_set(add_zero_rule(), mul_one_rule());
```

Each rule factory returns a fresh rule object. Because the rule's type includes the lambda types of its predicate and transformer, the returned type is stable across calls only if the lambdas are defined at namespace scope rather than inside the function body. In practice, defining the predicates and transformers as named functions or as `static inline` lambdas at namespace scope ensures that the rule type is identical across calls, which can matter when composing sets in templated contexts.

When two sets must be applied in sequence with different fixpoint semantics — for example, a normalization pass followed by a simplification pass — the straightforward approach is to call `apply` twice, feeding the output of the first into the second. Each call runs its own fixpoint loop. This is less efficient than merging the sets, but it preserves the intended phase separation and avoids ordering surprises between the two rule groups.

## 14.19 Observing the Passes

DSLtk does not provide built-in tracing of the fixpoint loop, but the structure of `RewriteSet` makes tracing straightforward to add by wrapping the rules. A tracing wrapper can be written as a rule whose predicate delegates to the wrapped rule and whose transformer logs before returning.

```cpp
template <typename Rule>
auto trace(const Rule& rule, const char* name) {
  return dsl::rule<"trace">(
      [rule](const dsl::ASTNode& n) { return rule.try_apply(n).has_value(); },
      [rule, name](const dsl::ASTNode& n) {
        auto result = *rule.try_apply(n);
        std::cerr << "rule " << name << " fired: " << n.dump()
                  << " -> " << result.dump() << "\n";
        return result;
      });
}
```

This wrapper calls the underlying rule's `try_apply` twice, once in the predicate and once in the transformer, which is wasteful but correct for diagnostic purposes. A more efficient tracer would cache the result, but the simple form suffices for interactive debugging. Wrapping each rule with `trace` before adding it to the set produces a log of every firing, which reveals the cascade order and the number of passes.

Because the `RewriteSet` type depends on the wrapped rule types, the traced set has a different type from the unwrapped one. The tracing is therefore a development-time tool, typically removed before the set is committed to production use. The `max_iterations` cap, combined with tracing, is usually enough to diagnose any convergence problem.

## 14.20 A Worked Multi-Pass Example

The example below exercises a set that requires several passes to converge, illustrating the fixpoint loop in action. The set combines constant folding with a rule that exposes new folding opportunities only after an identity is stripped.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto rs = dsl::rewrite_set(
      dsl::rule<"fold-add">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "add" && n.size() == 2
                && n.child(0).tag() == "num" && n.child(1).tag() == "num";
          },
          [](const dsl::ASTNode& n) {
            int a = std::stoi(n.child(0).value());
            int b = std::stoi(n.child(1).value());
            return dsl::leaf<"num">(std::to_string(a + b));
          }),
      dsl::rule<"add-zero">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "add" && n.size() == 2
                && n.child(1).tag() == "num" && n.child(1).value() == "0";
          },
          [](const dsl::ASTNode& n) { return n.child(0); }));
  // (+ (+ 2 3) 0)
  auto in = dsl::node<"add">(
      dsl::node<"add">(dsl::leaf<"num">(2), dsl::leaf<"num">(3)),
      dsl::leaf<"num">(0));
  std::cout << rs.apply(in).dump() << "\n";
}
```

The first pass descends to the inner `(+ 2 3)` and folds it to `5`. The outer node is rebuilt as `(+ 5 0)`. The `add-zero` rule then fires on `(+ 5 0)` within the same pass, because the right child is `0`, and replaces the node with `5`. The pass completes with `changed == true`. The second pass examines `5`, finds no rule fires, and returns `changed == false`. The loop exits and prints `5`.

Although this tree converges in a single pass thanks to bottom-up evaluation, a deeper tree such as `(+ (+ (+ 2 3) 0) 0)` exercises the same cascade at each level. The bottom-up traversal folds the innermost literal first, then strips the surrounding zeros level by level, all within one pass. Multi-pass convergence is most readily observed when a rule's output creates a match for a rule declared earlier in the set, which the current pass has already passed by. In that case, the next pass picks up the new match.

## 14.21 The Semantics of "Fixpoint" in This Design

The term "fixpoint" in DSLtk's rewrite machinery means specifically that a complete bottom-up pass over the tree produced no firings. It does not mean that the tree is a global normal form in the term-rewriting sense, because the rule set may be non-confluent — two different rewriting orders might lead to two different stable trees. The bottom-up, ordered application picks one specific order, and the fixpoint is the stable point reached by that order.

Confluence is the user's to ensure. If two rules can both fire on the same node and lead to different stable forms, the order in the `rewrite_set` call decides which form is reached. The fixpoint loop does not explore alternatives; it commits to the cascade as it unfolds. For simplification rule sets, which are usually oriented toward shrinkage, confluence tends to hold naturally because shrinkage rules converge on a unique minimal form. For normalization rule sets with multiple canonical targets, the user must verify that the chosen order yields the intended target.

The fixpoint is also local to the rule set. Adding a rule can change the fixpoint of a tree that previously converged, because the new rule may fire on the previously stable form. This is expected: the fixpoint is defined relative to the set, not absolutely. When composing a new rule into an existing set, the user should re-test convergence on representative inputs.

## 14.22 Performance Considerations

The cost of `apply` is the cost of one pass multiplied by the number of passes to convergence. A pass visits every node once, rebuilds every non-leaf node, and invokes each predicate once per node. The per-node cost is therefore the sum of the predicate costs plus the rebuild cost, which is proportional to the number of children. For a tree of `n` nodes and a set of `k` rules, a pass costs `O(n * k)` predicate evaluations plus `O(n)` node rebuilds.

The number of passes is bounded by `max_iterations` and, for terminating sets, by the number of times a rule can usefully fire. For shrinkage rules, this is at most the initial node count, but in practice it is a small constant — typically one to five passes for trees of moderate depth. The combination yields a practical cost that is linear in the tree size for typical sets, with a modest constant factor.

Predicate design is the dominant factor in constant factor. A predicate that examines only the node's tag and immediate children, using the accessors documented in Chapter 10, is `O(1)` in the subtree size. A predicate that calls `dump()` to compare subtrees, as the `add-self` rule earlier does, is `O(subtree size)` and can dominate the cost for large subtrees. Where possible, predicates should test structural properties directly rather than serializing subtrees for comparison.

Node rebuild cost is inherent to the immutable `ASTNode` design and cannot be avoided within the rewrite pass. The rebuilds are moves, not deep copies, so the per-rebuild cost is proportional to the number of children, not to the subtree size. The total allocation traffic is therefore proportional to the tree size times the number of passes, which is acceptable for the tree sizes typical of embedded DSL work.

## 14.23 Relationship to Chapter 13

Chapter 13 defined the `RewriteRule` and its `try_apply` member, which `RewriteSet` calls. The rule is the unit of transformation; the set is the driver. The two are designed to compose: a rule is self-contained, knowing nothing about the set it belongs to, and the set is agnostic to the rules' internal logic, calling `try_apply` uniformly via the fold expression.

The `dsl::rule<Tag>(pred, trans)` factory remains the only way to construct rules for use in a set. The tag, a `FixedString` non-type template parameter as described in Chapter 4, is currently used for identification and tracing; it does not affect the application order, which is determined solely by the order of arguments to `rewrite_set`. Tags are nevertheless valuable for diagnostics, because they let the user name rules in error messages and traces.

The predicate and transformer signatures match those documented in Chapter 13: `pred(const ASTNode&) -> bool` and `trans(const ASTNode&) -> ASTNode`. The `try_apply` method returns `std::optional<ASTNode>`, which `RewriteSet`'s fold inspects via `if (auto result = r.try_apply(current))`. This protocol is the contract between the rule and the set, and it is the only coupling between them.

## 14.24 Summary

A `RewriteSet` is a bundle of `RewriteRule` objects applied together to a tree until a fixpoint is reached. The set stores its rules in a `std::tuple` and applies them via a bottom-up pass that rewrites children before parents and cascades rules within each node visit. The fixpoint loop re-runs the pass until a complete pass produces no change, bounded by a configurable `max_iterations` cap.

Rule ordering matters because rules cascade: an earlier rule transforms a node before later rules see it, which can enable or suppress later matches. Bottom-up evaluation ensures that children are in a pass-local normal form before a parent is examined, which is essential for constant folding and similar rules. The `rewrite_set` factory constructs sets, and the `Rewrite` feature tag exposes a `rewrite` member to DSL hosts through the mixin pattern.

Termination is the user's responsibility. Shrinkage rules terminate naturally because each firing reduces a well-founded measure. Normalization rules that preserve size must be oriented toward a unique canonical form to avoid cycles. The `max_iterations` cap backstops non-terminating sets, and tracing wrappers can diagnose convergence problems. With these caveats, `RewriteSet` provides a simple, composable foundation for tree simplification and normalization within DSLtk, integrating cleanly with the `ASTNode` value type of Chapter 10, the builders of Chapter 11, the traversal idioms of Chapter 12, and the rule definitions of Chapter 13.
