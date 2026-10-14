# Chapter 16: `BinExpr`, `UnaryExpr`, and Fused Evaluation

Chapter 15 introduced the expression-template facility of DSLtk as a mechanism for building lazy arithmetic trees over types that derive from `dsl::DSL` with the `dsl::ExprTemplates` feature. The chapter ended at the point where overloaded operators return opaque node types, deferred until the caller explicitly asks for a result. This chapter opens those node types and explains exactly how evaluation proceeds.

Two node types form the spine of every expression tree: `BinExpr<Op, L, R>`, returned by every binary operator, and `UnaryExpr<Op, E>`, returned by unary negation. Both are trivial aggregates that store their operands and an operation tag, and both expose a `constexpr` `eval()` member that walks the subtree in a single fused pass. Understanding these two types is sufficient to predict the behaviour of any expression built through the `ExprTemplates` mixin.

The chapter also covers the operation tag types supplied by the standard library, the `eval_operand` helper that unifies leaf and inner-node evaluation, the `ExprLike` concept that constrains the leaves, and the free-function operator overloads that permit `BinExpr` nodes to appear on the left-hand side of further operators. Worked examples range from scalar arithmetic to element-wise vector operations, and a closing section catalogues the pitfalls that accompany reference-deferred evaluation.

## Recap: The `ExprTemplates` Feature

Before describing the node types it is useful to recap the surface API presented by the `ExprTemplates` feature, introduced in Chapter 15. A DSL type derives from `dsl::DSL<Derived, dsl::ExprTemplates>` and supplies a `expr_value()` member that returns the underlying value. The feature's mixin injects binary `operator+`, `operator-`, `operator*`, and `operator/` as hidden friends, plus a unary `operator-` for negation. None of these operators compute a result; each constructs and returns a node.

The node types live at namespace scope in `dsl` so that they can be mentioned in user code when needed, although in practice `auto` is almost always sufficient. The binary operators return `BinExpr<std::plus<>, Derived, R>` and analogous specialisations, while unary negation returns `UnaryExpr<std::negate<>, Derived>`. The operators are `constexpr`, so the resulting tree can be built at compile time.

```cpp
struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

Scalar a{2}, b{3}, c{4};
auto expr = a + b * c - a;   // type: BinExpr<std::minus<>, BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>, Scalar>
double r = expr.eval();      // 2 + 3*4 - 2 = 12
```

The example above, drawn from `examples/12-expression-templates.cpp`, exhibits the central contract of this chapter: the operators build a tree, and `eval()` collapses it into a single value. The remainder of the chapter explains how that collapse happens.

## The `BinExpr` Node Type

`BinExpr` is the node type returned by every binary operator in the `ExprTemplates` mixin. It is a class template with three parameters: the operation tag `Op`, the left operand type `L`, and the right operand type `R`. The class stores copies of both operands as members named `lhs` and `rhs`, and constructs them by move from the constructor's by-value parameters.

```cpp
template <typename Op, typename L, typename R> struct BinExpr
{
  L lhs;
  R rhs;

  constexpr BinExpr (L l, R r) : lhs (std::move (l)), rhs (std::move (r)) {}
  // ... eval() described below ...
};
```

Because `L` and `R` are deduced from the arguments passed to the operator, a `BinExpr` node can hold leaves of the derived DSL type, other `BinExpr` nodes, or `UnaryExpr` nodes, all uniformly. The node itself is an aggregate with no further logic beyond construction and evaluation, which keeps its layout minimal and makes deeply nested trees cheap to instantiate.

The operation tag `Op` is not stored as a data member; it is a type parameter only. The node carries the operation in its static type, and at evaluation time a temporary `Op{}` is constructed and invoked. This is the standard tag/functor pattern: `std::plus<>`, `std::minus<>`, `std::multiplies<>`, and `std::divides<>` are all empty classes whose call operator performs the corresponding arithmetic. Carrying the operation in the type system rather than in a data member keeps every `BinExpr` the size of just its two operands.

The constructor is `constexpr`, so a `BinExpr` can be assembled during constant evaluation. The members `lhs` and `rhs` are public, which permits introspection of an expression tree by generic code — for example, a visitor that walks the tree for diagnostic purposes can read `node.lhs` and `node.rhs` directly. The primary intended access path, however, is `eval()`.

## The `UnaryExpr` Node Type

`UnaryExpr` is the companion node type for unary operations. The `ExprTemplates` mixin provides only one unary operator, negation, which returns `UnaryExpr<std::negate<>, Derived>`. Like `BinExpr`, the class template is parametrised by an operation tag and a single operand type `E`, and stores the operand by value in a public member named `operand`.

```cpp
template <typename Op, typename E> struct UnaryExpr
{
  E operand;

  explicit constexpr UnaryExpr (E e) : operand (std::move (e)) {}
  // ... eval() described below ...
};
```

The constructor is marked `explicit` to prevent implicit conversions from an operand into a unary expression, reflecting the fact that constructing a node is a deliberate act performed by an operator rather than by ordinary conversion. As with `BinExpr`, the operation tag is carried in the static type and is never stored as a data member.

Although the library only ships `std::negate<>` as a unary tag, the `UnaryExpr` template is general and will accept any callable taking a single argument. A user who wishes to introduce, for example, a logical-not node for a boolean DSL can construct `UnaryExpr<my_not_t, Bool>{x}` directly, or supply a custom operator that returns such a node. The evaluation machinery described below is agnostic to the choice of tag.

`UnaryExpr` participates in the same recursive evaluation as `BinExpr`. When a `UnaryExpr` appears as the operand of a `BinExpr`, the binary node's `eval_operand` helper detects the inner node's `eval()` method and recurses into it. No special handling is required to mix unary and binary nodes in a single tree.

## Operation Tags

The operation tags used by the `ExprTemplates` mixin are drawn from the standard library. `std::plus<>` represents addition, `std::minus<>` subtraction, `std::multiplies<>` multiplication, `std::divides<>` division, and `std::negate<>` unary negation. Each is an empty class with a `constexpr` call operator that forwards to the corresponding operator expression, and the transparent specialisation (`std::plus<>` rather than `std::plus<void>`) deduces its argument and return types from the call site.

The choice of standard-library tags has two consequences. First, the operation applied at evaluation is exactly `Op{}(l, r)` for binary nodes and `Op{}(x)` for unary nodes, which means that any type for which `operator+` and friends are defined is usable as a leaf. Second, because the tags are empty, they impose no size overhead on the node, and because they are trivial they participate cleanly in `constexpr` evaluation.

A tag must be defined for the value type returned by the leaves. For `Scalar` leaves, the value type is `double`, and all five standard tags are defined for `double`. For a vector leaf whose `expr_value()` returns `std::vector<float>`, the same tags work provided `std::vector<float>` supports the corresponding operator — and `std::vector` does not provide element-wise arithmetic out of the box, so a user-defined vector type with overloaded operators is required. The chapter returns to this point in the worked examples.

The tags are invoked by value-construction: `Op{}` constructs a temporary and immediately calls its call operator. This is the same pattern used by `std::transform` and the standard algorithms, so the semantics will be familiar. The tags are not stored, not configured, and not user-visible at the leaf level; they are purely an internal dispatch mechanism.

## The `eval()` Method: Verbatim Definition

The heart of this chapter is the `eval()` member of `BinExpr`. Its definition is short, and is reproduced verbatim from `DSLtk.hpp`:

```cpp
  constexpr auto
  eval () const
  {
    auto l = eval_operand (lhs);
    auto r = eval_operand (rhs);
    return Op{}(l, r);
  }
```

The method is `constexpr` and `const`-qualified, so it may be invoked on temporaries and during constant evaluation. It evaluates the left operand, then the right operand, then applies the operation tag to the pair. The order of evaluation of the two operands is the order in which they appear in the source: left, then right. This is a stronger guarantee than that provided by the built-in arithmetic operators, whose operand evaluation order is unspecified for most operators.

The corresponding member of `UnaryExpr` is structurally identical, with a single operand:

```cpp
  constexpr auto
  eval () const
  {
    if constexpr (requires { operand.eval (); })
      {
        return Op{}(operand.eval ());
      }
    else if constexpr (requires { operand.expr_value (); })
      {
        return Op{}(operand.expr_value ());
      }
    else
      {
        return Op{}(operand);
      }
  }
```

The `UnaryExpr` version inlines the dispatch that `BinExpr` factors into the `eval_operand` helper. Both formulations are equivalent in effect: evaluate the operand, then apply `Op` to the result. The duplication is deliberate, keeping each node type self-contained.

## The `eval_operand` Helper

`BinExpr` factors operand evaluation into a private static helper, `eval_operand`, templated on the operand type. The helper uses `if constexpr` with `requires` clauses to dispatch on three cases, in order: if the operand exposes an `eval()` method, call it; otherwise, if it exposes an `expr_value()` method, call that; otherwise, return the operand unchanged.

```cpp
  template <typename T>
  static constexpr auto
  eval_operand (const T &operand)
  {
    if constexpr (requires { operand.eval (); })
      {
        return operand.eval ();
      }
    else if constexpr (requires { operand.expr_value (); })
      {
        return operand.expr_value ();
      }
    else
      {
        return operand;
      }
  }
```

The first branch handles inner nodes — both `BinExpr` and `UnaryExpr` expose `eval()`, so a subtree is collapsed by a recursive call. The second branch handles leaves of the derived DSL type, which expose `expr_value()` but not `eval()`. The third branch handles raw values that have been woven into the tree, such as a plain `double` literal used as an operand; such a value is returned unchanged and is then passed directly to the operation tag.

The use of `requires` expressions makes the dispatch structural rather than tag-based. Any type that exposes an `eval()` method will be treated as an inner node, regardless of whether it is one of DSLtk's node types. This means a user-defined node type that supplies a conforming `eval()` can participate in the same tree without modification to the library. The same openness applies to leaves: any type with an `expr_value()` method can serve as a leaf, even if it does not derive from `dsl::DSL`.

The third branch is what permits raw values to appear in expressions. Although the `ExprTemplates` mixin only defines operators with the derived type on the left, the free-function overloads described later in the chapter accept any right-hand operand, including built-in numeric types. When such a raw value reaches `eval_operand`, the third branch returns it unchanged, and the operation tag is then applied to the pair of evaluated values.

## Recursive Evaluation: A Single Fused Pass

Evaluation proceeds by structural recursion from the root. The root node's `eval()` calls `eval_operand` on each of its children; for each child that is itself a node, `eval_operand` calls the child's `eval()`, which recurses in turn. The recursion terminates at leaves, where `eval_operand` takes the `expr_value()` branch and returns the underlying value. The operation tags are applied on the way back up the recursion, so the root's `Op` is applied last.

Consider the expression `a + b * c`. The multiplication binds tighter than the addition, so the tree is `BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>`. Calling `eval()` on the root evaluates the left leaf `a` through `expr_value()`, then evaluates the right subtree by calling its `eval()`, which in turn evaluates `b` and `c` and applies `std::multiplies<>`. Finally the root applies `std::plus<>` to `a` and the product `b * c`. The total work is one multiplication and one addition, in that order.

Because each node applies its operation after its children have been fully evaluated, the entire tree is walked in a single post-order pass. There is no staging: no intermediate vector of partial results is assembled, and no sub-expression is evaluated more than once. The pass is what the literature calls *fused*: the operations are combined at the point of evaluation rather than materialised as separate results.

The recursion depth is proportional to the height of the tree, which for a left-associative chain of `n` binary operators is `n`. For typical DSLtk usage, where expressions are short, this depth is negligible. For very long chains, the recursion is still bounded by the expression's syntactic nesting, not by any data structure size, so stack exhaustion is not a practical concern.

## Fusion Versus Eager Evaluation

The defining contrast between expression templates and eager evaluation is the treatment of intermediate results. In eager evaluation, each operator computes and materialises a result: `a + b * c` would first compute `b * c` into a temporary, then compute `a + temp` into a second temporary. For scalar types the temporaries are cheap, but for vector or matrix types each temporary is a heap allocation and a full-length copy.

With `BinExpr` and `UnaryExpr`, no temporary is materialised for any sub-expression. The fused `eval()` pass reads each leaf once and combines the values on the stack. For a vector expression of length `n` with `k` operators, eager evaluation would allocate `k` temporaries of size `n`; the fused pass allocates none. The result is a single value, constructed in place from the leaves.

```cpp
struct Vec3 : dsl::DSL<Vec3, dsl::ExprTemplates> {
  std::array<float,3> v{};
  Vec3() = default;
  Vec3(float x, float y, float z) : v{x, y, z} {}
  std::array<float,3> expr_value() const { return v; }
};

Vec3 u{1,2,3}, w{4,5,6};
auto sum = u + w;                       // BinExpr<std::plus<>, Vec3, Vec3>
auto r = sum.eval();                    // std::array<float,3>{5,7,9}
```

This example relies on `std::array<float,3>` supporting `operator+`, which it does not in the standard library. A real vector type would either provide its own `operator+` returning a new vector, or would use a custom operation tag. The principle, however, is independent of the value type: the fused pass applies the tag once per node, with no intermediate storage.

The fusion guarantee is a property of the `eval()` implementation, not of the type system. Because `eval_operand` calls `eval()` on inner nodes immediately and applies the operation tag before returning, there is no point at which a sub-expression's result is stored except in the local variables `l` and `r` of the calling node. These locals are registers or small stack slots for scalar types, and move-constructed values for larger types.

## The Return Type of `eval()`

The return type of `eval()` is deduced by `auto` from the body of the method. For a `BinExpr` whose leaves return `double` from `expr_value()`, the `eval_operand` calls yield `double`, the operation tag `std::plus<>` returns `double`, and `eval()` returns `double`. For a `UnaryExpr<std::negate<>, Scalar>`, the same reasoning yields `double`.

The return type is therefore the common type of the operation tag's result over the operand value types. For homogeneous trees — all leaves of the same type — this is simply the leaf value type. For heterogeneous trees, where leaves of different types are combined, the return type is whatever `Op{}(l, r)` yields, which follows the usual arithmetic-conversion rules of the operation being invoked.

```cpp
Scalar a{2}, b{3};
auto e1 = a + b;                       // BinExpr<std::plus<>, Scalar, Scalar>
static_assert(std::is_same_v<decltype(e1.eval()), double>);

auto e2 = -a;                          // UnaryExpr<std::negate<>, Scalar>
static_assert(std::is_same_v<decltype(e2.eval()), double>);
```

Because the return type is deduced, it is not possible to write a forward declaration of `eval()` without a trailing return type. Users who need to name the return type should use `decltype(expr.eval())` rather than attempting to spell it out. This is the same idiom used throughout the standard library for deduced-return-type functions.

The return type is preserved across nesting. A `BinExpr` whose right operand is a `BinExpr` returns the type of the outer operation applied to the leaf type and the inner operation's return type. As long as the operations are defined for the relevant types, the deduction chains cleanly and the final return type is the leaf value type (or its arithmetic-promoted equivalent).

## The `ExprLike` Concept

DSLtk provides a concept, `ExprLike`, that characterises types usable as leaves in an expression template tree. The concept is defined near the bottom of `DSLtk.hpp` and requires two things: the type must have the `ExprTemplates` feature, and it must expose a `const`-qualified `expr_value()` method.

```cpp
template <typename T>
concept ExprLike = HasFeature<T, ExprTemplates>
                   && requires (const T &t) { t.expr_value (); };
```

The first clause, `HasFeature<T, ExprTemplates>`, is the feature-tag check introduced in Chapter 5: it verifies that `T` derives from `dsl::DSL<T, dsl::ExprTemplates>` (or otherwise registers the feature). The second clause is a `requires` expression that checks the existence of `expr_value()`. Together they capture exactly the contract that the `ExprTemplates` mixin imposes on its derived class.

`ExprLike` is the bridge between the user-facing DSL type and the internal node machinery. Although `BinExpr` and `UnaryExpr` do not themselves constrain their operands with `ExprLike` — they accept any type that supports the required methods — the concept is the intended way to document and constrain generic code that builds expressions. A function that accepts a leaf and returns an expression can constrain its parameter with `ExprLike` to give clear diagnostics when passed an inappropriate type.

The concept does not constrain the return type of `expr_value()`. Any return type is acceptable, on the understanding that the operation tags used in the tree must be defined for that type. This keeps the concept structural and avoids coupling it to a particular value type. A DSL whose leaves return `int` and a DSL whose leaves return a user-defined matrix type both satisfy `ExprLike` equally.

## The `ExprTerminal` Tag

`DSLtk.hpp` declares a small empty struct, `ExprTerminal`, that serves as a tag type indicating a terminal in an expression template tree. The tag itself is not used by the `ExprTemplates` mixin or by the node types described in this chapter; it is provided as a hook for user code that wishes to mark or detect leaves by trait.

```cpp
struct ExprTerminal
{
};
```

A user-defined leaf type may, if it wishes, derive from `ExprTerminal` or specialise a trait on it to integrate with custom tree-walking code. The library's own evaluation machinery does not consult the tag: the `eval_operand` helper dispatches on the presence of `eval()` and `expr_value()` methods, not on inheritance from `ExprTerminal`. The tag is therefore a documentation and extension point rather than a functional requirement.

The existence of `ExprTerminal` in the header is a hint that the library anticipates third-party extension. A tree visitor that wishes to distinguish leaves from inner nodes generically can check `std::is_base_of_v<ExprTerminal, T>`, provided the leaf types cooperate by deriving from the tag. Leaves written in the canonical style of `Scalar` do not need to do so, because the `eval_operand` dispatch already distinguishes them by method signature.

In practice, most user code will never mention `ExprTerminal`. It is mentioned here for completeness, and so that readers of the header are not surprised by its presence. The functional contract of a leaf is the `expr_value()` method plus the `ExprTemplates` feature, neither of which involves the terminal tag.

## `BinExpr` on the Left: Free-Function Overloads

The `ExprTemplates` mixin defines its binary operators as hidden friends inside the derived class. This means the operators fire when the derived type appears on the left of the operator, but they do not fire when a `BinExpr` node appears on the left. Without further support, an expression like `(a + b) * c` would fail to compile: the left operand is a `BinExpr`, not a `Scalar`, and no `operator*` accepts a `BinExpr` on the left.

DSLtk closes this gap with four free-function operator overloads at namespace scope, one for each binary operator. Each overload accepts a `BinExpr<Op1, L1, R1>` on the left and an arbitrary right operand, and returns a new `BinExpr` whose left operand is the inner `BinExpr`.

```cpp
template <typename Op1, typename L1, typename R1, typename R>
constexpr auto
operator* (const BinExpr<Op1, L1, R1> &lhs, const R &rhs)
{
  return BinExpr<std::multiplies<>, BinExpr<Op1, L1, R1>, R>{ lhs, rhs };
}
```

The four overloads cover `operator+`, `operator-`, `operator*`, and `operator/`. They are symmetrical in shape: each constructs a `BinExpr` whose operation tag is the matching standard-library tag, whose left type is the incoming `BinExpr`, and whose right type is the deduced `R`. The inner `BinExpr` is copied into the new node by value.

These overloads are what make left-associative chains like `a + b * c - a` evaluate correctly. The expression parses as `(a + (b * c)) - a`: the inner `b * c` is a `BinExpr`, which then appears on the left of the subtraction. The free-function `operator-` for `BinExpr` on the left fires and wraps the inner node in an outer `BinExpr<std::minus<>, ...>`. Without the free functions, such chains would have to be written with explicit nesting.

The free functions only cover `BinExpr` on the left; they do not cover `UnaryExpr` on the left. An expression like `(-a) + b` parses as `UnaryExpr<...> + Scalar`, and the `operator+` hidden friend of `Scalar` does not fire because the left operand is not a `Scalar`. In practice this is rarely a problem because unary negation usually binds to a single leaf, but it is a limitation worth noting. A user who needs `UnaryExpr` on the left can write a custom overload, or can restructure the expression so that the unary node appears on the right.

## Worked Example: Scalar Arithmetic

The simplest non-trivial expression is the one from `examples/12-expression-templates.cpp`. Three `Scalar` leaves are combined with three operators, and the result is printed.

```cpp
struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

int main() {
  Scalar a{2}, b{3}, c{4};
  auto expr = a + b * c - a;
  std::cout << expr.eval() << "\n";   // prints 12
}
```

The type of `expr` is, fully expanded, `BinExpr<std::minus<>, BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>, Scalar>`. The multiplication is the innermost node, wrapped by the addition, wrapped by the subtraction. The evaluation walks this tree in a single pass: `b * c` is computed first (yielding 12), then `a + 12` (yielding 14), then `14 - a` (yielding 12).

The absence of intermediate temporaries is invisible at the scalar level because a `double` temporary is free. The value of the example is pedagogical: it shows the shape of the tree and the order of operations in the smallest possible setting. The same shape, applied to a vector type, is where fusion pays off.

Note that the operands are stored by value inside each `BinExpr`. The three `Scalar` leaves are copied into the inner multiplication node, the result of which is a node (not a value) that is copied into the addition node, and so on. The copies are cheap for `Scalar` — eight bytes each — but for a larger leaf type the cost of copying the leaf into every node that references it would be significant. This is the reason the library stores operands by value rather than by reference: it keeps the nodes self-contained and safe to return from functions, at the cost of copying leaves that appear multiple times.

## Worked Example: Nested Unary Negation

Unary negation inserts a `UnaryExpr` node into the tree. The simplest case is negation of a single leaf, but the more interesting case is negation combined with binary operators, where the recursion must descend through both node types.

```cpp
Scalar a{5}, b{2};
auto expr = -a + b;
std::cout << expr.eval() << "\n";   // prints -3
```

The type of `expr` is `BinExpr<std::plus<>, UnaryExpr<std::negate<>, Scalar>, Scalar>`. Evaluation of the root calls `eval_operand` on the left, which is a `UnaryExpr`. The `UnaryExpr` exposes `eval()`, so `eval_operand` calls it; that call in turn evaluates the inner `Scalar` leaf through `expr_value()` and applies `std::negate<>` to yield `-5`. The right operand is a plain `Scalar`, evaluated through `expr_value()` to `2`. The root applies `std::plus<>` to yield `-3`.

The example demonstrates that `UnaryExpr` and `BinExpr` interoperate without any special glue. The `eval_operand` helper's first branch — `if constexpr (requires { operand.eval(); })` — accepts any type with an `eval()` method, regardless of whether it is `BinExpr` or `UnaryExpr`. This is the structural dispatch that makes the node types composable.

A deeper nesting works the same way. An expression like `-(-a + b) * c` produces a tree with a `UnaryExpr` at the root, a `BinExpr` below it, and a further `UnaryExpr` inside that. Evaluation still proceeds as a single post-order walk: the innermost `UnaryExpr` evaluates first, then the `BinExpr`, then the outer `UnaryExpr`, then the multiplication at the root. Each level applies its operation tag to the result of its children.

## Worked Example: Element-Wise Vector Arithmetic

To illustrate fusion on a type where intermediates would be costly, consider a small fixed-size vector type. The leaf's `expr_value()` returns the vector by value, and the operation tags must be defined for the vector type. Because `std::array` does not provide element-wise arithmetic, the example uses a custom `Vec` type with overloaded operators.

```cpp
struct Vec {
  std::array<float,3> v{};
  Vec() = default;
  Vec(float x, float y, float z) : v{x, y, z} {}
  float& operator[](std::size_t i) { return v[i]; }
  const float& operator[](std::size_t i) const { return v[i]; }
};

constexpr Vec operator+(const Vec& a, const Vec& b) {
  return Vec{a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}
constexpr Vec operator-(const Vec& a, const Vec& b) {
  return Vec{a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}
constexpr Vec operator*(const Vec& a, const Vec& b) {
  return Vec{a[0]*b[0], a[1]*b[1], a[2]*b[2]};
}
constexpr Vec operator-(const Vec& a) {
  return Vec{-a[0], -a[1], -a[2]};
}
```

With these operators in scope, `std::plus<>` and friends dispatch to the element-wise overloads when invoked on `Vec` values. A leaf type wrapping `Vec` is then straightforward.

```cpp
struct VecLeaf : dsl::DSL<VecLeaf, dsl::ExprTemplates> {
  Vec v{};
  VecLeaf() = default;
  VecLeaf(float x, float y, float z) : v{x, y, z} {}
  Vec expr_value() const { return v; }
};

VecLeaf u{1,2,3}, w{4,5,6}, x{7,8,9};
auto expr = u + w * x - u;
Vec r = expr.eval();                  // {1+4*7-1, 2+5*8-2, 3+6*9-3} = {28, 40, 54}
```

In eager evaluation, the expression `u + w * x - u` would allocate three temporary `Vec` objects: one for `w * x`, one for `u + temp1`, and one for `temp2 - u`. With the fused `eval()` pass, no such temporaries are allocated. The single post-order walk reads each leaf once and produces the final `Vec` directly. For a three-element vector the saving is trivial; for a leaf wrapping a large `std::vector<float>` it is the difference between `O(k)` allocations and zero.

The example also shows that the operation tags are not coupled to the leaf type. The `ExprTemplates` mixin hardcodes `std::plus<>`, `std::minus<>`, `std::multiplies<>`, and `std::divides<>`, but these transparent functors dispatch to whatever `operator+` and so on are found by overload resolution at the call site. A leaf that returns a custom value type from `expr_value()` therefore gets the right arithmetic for free, provided the corresponding operators are defined.

## `constexpr` Evaluation

Every operation in the node types is marked `constexpr`: the constructors, the `eval()` methods, the `eval_operand` helper, and the free-function operator overloads. The operation tags from the standard library are also `constexpr`. Consequently, an entire expression tree can be built and evaluated at compile time, provided the leaf types and their `expr_value()` methods are themselves `constexpr`.

```cpp
constexpr Scalar ca{2}, cb{3}, cc{4};
constexpr auto cexpr = ca + cb * cc - ca;
static_assert(cexpr.eval() == 12);
```

This capability is what permits expression templates to be used in template parameters and other constant-expression contexts. A `static_assert` that checks an arithmetic identity, a non-type template parameter whose value is computed from an expression, or a `constexpr` variable initialised from an `eval()` call are all viable uses.

The requirements for compile-time evaluation are the usual ones: no non-`constexpr` operations may appear in the call tree, and all functions invoked must be reachable at compile time. For the standard operation tags and built-in arithmetic types these requirements are met automatically. For a user-defined value type, the type's arithmetic operators and its `expr_value()` method must be `constexpr`, and the type must be a literal type.

Compile-time evaluation does not change the fused character of the pass. The recursion unfolds at compile time just as it would at run time, and the result is a single constant value with no intermediate temporaries materialised in the compiled program. In favourable cases the compiler can fold the entire expression into a single constant, eliminating all trace of the tree from the generated code.

## A Larger Expression: Tracing the Pass

To consolidate the picture of a single fused pass, consider a slightly larger expression and trace its evaluation step by step. The expression is `a + b * c - a / b`, with `a`, `b`, `c` of type `Scalar`.

```cpp
Scalar a{6}, b{3}, c{4};
auto expr = a + b * c - a / b;
std::cout << expr.eval() << "\n";   // 6 + 3*4 - 6/3 = 6 + 12 - 2 = 16
```

The tree, parsing with standard precedence, is `(a + (b * c)) - (a / b)`. The root is a `BinExpr<std::minus<>>` whose left operand is a `BinExpr<std::plus<>>` and whose right operand is a `BinExpr<std::divides<>>`. Evaluation proceeds as follows.

The root's `eval()` calls `eval_operand(lhs)`, where `lhs` is the plus-node. The plus-node's `eval()` is invoked, which calls `eval_operand` on its own left (the leaf `a`, returning 6) and its own right (the multiplication node, whose `eval()` returns 12), and applies `std::plus<>` to yield 18. Control returns to the root, which now calls `eval_operand(rhs)` on the divides-node. The divides-node's `eval()` evaluates `a` (6) and `b` (3) and applies `std::divides<>` to yield 2. Finally the root applies `std::minus<>` to 18 and 2, yielding 16.

The trace exhibits three properties that hold for every expression. First, each leaf is read exactly once per path through the tree; a leaf that appears twice in the expression (here `a` and `b`) is read once per appearance, not cached. Second, operations are applied in post-order: children before parents. Third, the entire computation uses only the local variables `l` and `r` of each active `eval()` frame; no auxiliary storage is allocated.

The trace also shows that the order of operations is determined entirely by the structure of the tree, which in turn is determined by the precedence and associativity of the C++ operators. Because `operator*` and `operator/` bind tighter than `operator+` and `operator-`, and because the binary operators are left-associative, the tree has the shape one would expect from reading the expression with standard mathematical precedence. There is no reassociation or optimisation; the tree is evaluated as written.

## Mixing Leaves and Raw Values

The third branch of `eval_operand` — the fallback that returns the operand unchanged — allows raw values to participate in expressions. Although the `ExprTemplates` mixin's hidden-friend operators require a `Derived` on the left, the free-function overloads accept any right-hand operand, including a built-in numeric literal. The result is a `BinExpr` whose right operand type is a raw scalar.

```cpp
Scalar a{2};
auto expr = a * 3.0;                  // BinExpr<std::multiplies<>, Scalar, double>
std::cout << expr.eval() << "\n";     // 6.0
```

When `eval()` runs, `eval_operand(lhs)` calls `a.expr_value()` and yields `2.0`; `eval_operand(rhs)` takes the third branch and returns `3.0` unchanged. `std::multiplies<>{}` applied to the pair yields `6.0`. The raw value bypasses both the `eval()` and `expr_value()` branches because it has neither method.

This facility is convenient for scaling and similar operations, but it has a sharp edge. The raw value is stored by value inside the `BinExpr`, so it is copied in at construction time. For a `double` this is free, but for a large raw value — say, a `std::vector<float>` passed on the right of an operator — the copy can be expensive. More importantly, the operation tag must be defined for the pair of types involved: `std::multiplies<>{}` applied to a `double` and a `double` works, but applied to a `Vec` and a `double` it requires a corresponding `operator*(const Vec&, double)`, which the user must supply.

The mixing of raw values and leaves also affects the return type of `eval()`. When a raw `double` is combined with a `Scalar` whose `expr_value()` returns `double`, the result is `double`. When a raw `int` is combined with a `Scalar`, the result follows the usual arithmetic conversions: `std::plus<>{}` applied to `double` and `int` yields `double`. The return type is always deduced from the actual `Op{}(l, r)` invocation, so the standard conversion rules apply directly.

## Pitfalls: Operand Lifetimes and Value Semantics

The node types store their operands by value. This is a deliberate choice: it makes nodes self-contained, safe to return from functions, and independent of the lifetime of the original leaves. A `BinExpr` returned from a function carries its own copies of its leaves, and can be evaluated long after the original leaves have gone out of scope.

The cost of this safety is that leaves are copied into every node that references them. A leaf that appears `n` times in an expression is copied `n` times, once into each node. For a small leaf type like `Scalar` this is negligible, but for a leaf wrapping a large container the cost can be significant. Users for whom this cost matters have two options: make the leaf cheap to copy (for example, by wrapping a `std::shared_ptr` to the underlying data), or restructure the expression to reuse a single sub-expression via `auto`.

```cpp
Scalar a{2}, b{3};
auto bc = b * c;          // a named sub-expression, evaluated once
auto expr = a + bc - bc;  // bc copied twice into the tree
```

The example above illustrates the trade-off. Naming `b * c` as `bc` and then using `bc` twice in the outer expression causes the `bc` node to be copied twice into the outer tree. Each copy is a shallow copy of the `BinExpr`, which in turn copies its `Scalar` operands. If `bc` is large, this is wasteful; if it is small, it is harmless. There is no reference-mode alternative in the library, so the trade-off must be managed by the user.

A related pitfall applies to leaf types that themselves hold references or views. Although the node copies the leaf, the leaf's copy constructor may produce a shallow copy that shares data with the original. If the shared data is destroyed before the node is evaluated, the node will hold a dangling reference. The library cannot protect against this; it is the user's responsibility to ensure that leaf types are either deep-copying or that the underlying data outlives the node.

The by-value storage model also means that mutating a leaf after constructing an expression has no effect on the expression's result. The expression captures the leaf's value at construction time, not a reference to the leaf. This is usually the desired behaviour — it makes expressions referentially transparent — but it can surprise users who expect later mutations to be visible.

## Pitfalls: Deeply Nested Types

The type of a `BinExpr` expression grows with the depth of the tree. A chain of `n` binary operators produces a type whose fully expanded spelling has `O(n)` template arguments nested to depth `O(n)`. Compiler diagnostics that attempt to print this type can become unreadable for even moderately sized expressions, and error messages that mention the type by name can stretch for many lines.

The standard remedy is to use `auto` for every expression variable. The `ExprTemplates` design is built around type deduction: the operators return deduced types, the free-function overloads return deduced types, and `eval()` returns a deduced type. There is rarely a reason to name a `BinExpr` type explicitly, and doing so should be avoided.

```cpp
Scalar a{1}, b{2}, c{3}, d{4};
auto expr = a + b - c + d;            // do not spell this type
auto r = expr.eval();
```

When a function must accept an arbitrary expression as a parameter, the usual idiom is a template with a deduced type parameter, optionally constrained by a concept. Because `ExprLike` constrains leaves rather than nodes, a function accepting an arbitrary node must use a weaker constraint, such as `requires (const T& t) { t.eval(); }`, or simply be an unconstrained template. The library does not provide a concept for "any expression node", so user code that needs one must define it locally.

Deeply nested types can also affect compile times, particularly when the same expression is instantiated many times in different contexts. The cost is usually modest, but for expressions built inside tight generic loops over many types it can become noticeable. As with all template-heavy code, the remedy is to factor common expressions into named functions whose bodies are compiled once.

## Pitfalls: The Operation Must Be Defined

The `ExprTemplates` mixin hardcodes the operation tags as `std::plus<>`, `std::minus<>`, `std::multiplies<>`, `std::divides<>`, and `std::negate<>`. These transparent functors dispatch to the corresponding operator overloads found by normal lookup. If the value type returned by a leaf's `expr_value()` does not support the required operator, the expression will fail to compile at the point where `eval()` is instantiated.

```cpp
struct StringLeaf : dsl::DSL<StringLeaf, dsl::ExprTemplates> {
  std::string s;
  std::string expr_value() const { return s; }
};

StringLeaf p{"hello"}, q{"world"};
auto expr = p + q;        // ok: std::string supports operator+
// auto bad = p * q;      // error: std::string has no operator*
```

The error message for an undefined operation is usually clear, because it originates from the body of `eval()` where `Op{}(l, r)` is instantiated. The fault is reported at the point where the operation is attempted, not at the point where the tree is built, because the operators that build the tree do not inspect the value type. This is consistent with the lazy philosophy: construction never fails, only evaluation does.

A consequence is that the set of usable operations on a given leaf type is determined entirely by the value type's operator overloads. To extend the set, the user provides additional operator overloads for the value type; the `ExprTemplates` machinery picks them up automatically because the standard tags are transparent. There is no need to register operations with the library or to specialise the tags.

A subtler pitfall concerns the choice of value type returned by `expr_value()`. If `expr_value()` returns by value, as in the `Scalar` example, each call materialises a copy. If it returns by reference, the leaf avoids the copy but exposes the leaf's internal storage to the operation tag. For types where the operation modifies its arguments — which the standard tags do not — returning by reference would be a hazard. The standard tags are pure, so the choice is purely one of performance.

## Summary

`BinExpr<Op, L, R>` and `UnaryExpr<Op, E>` are the two node types that back every expression built through the `dsl::ExprTemplates` feature introduced in Chapter 15. Each stores its operands by value and carries its operation as a type parameter, drawing on the standard tags `std::plus<>`, `std::minus<>`, `std::multiplies<>`, `std::divides<>`, and `std::negate<>`. Evaluation is performed by a `constexpr` `eval()` method that recursively walks the tree in a single post-order pass, applying each operation tag on the way back up.

The `eval_operand` helper, with its three-way `if constexpr` dispatch on `eval()`, `expr_value()`, and fallback, unifies inner nodes, leaves, and raw values into a single evaluation path. The result is a fused pass: no intermediate temporaries are materialised for sub-expressions, and the entire tree is collapsed into one value of the leaf's value type. The `ExprLike` concept, built on the feature-tag machinery of Chapter 5, characterises the leaf types that participate in this scheme.

The free-function operator overloads permit `BinExpr` nodes to appear on the left of further operators, enabling left-associative chains to be written naturally. The whole system is `constexpr`, so expression trees can be built and evaluated at compile time. The principal pitfalls are the by-value storage of operands (which copies leaves into every referencing node), the rapid growth of type spelling with tree depth (which argues for `auto`), and the requirement that the operation tag be defined for the leaf's value type (which is the user's responsibility). With these caveats observed, `BinExpr` and `UnaryExpr` provide a small, complete, and composable foundation for lazy arithmetic over any type that satisfies `ExprLike`.
