# Chapter 15: Expression Templates: Lazy Trees

An ordinary arithmetic expression such as `a + b * c` evaluates eagerly: the compiler emits code that multiplies `b` by `c`, stores the product in a temporary, adds `a`, and returns the result. For scalar values this is exactly what the programmer wants. For richer domain types — symbolic scalars, vector expressions, AST fragments, parser costs — eager evaluation throws away the *shape* of the computation. The intermediate temporary has no record that it was once a product; the addition only sees a value. Once evaluated, the structure is gone.

Expression templates solve this problem at the type level. Instead of computing a result, each operator application builds a small node that *records* the operation and refers to its operands. The full expression `a + b * c - a` becomes a compile-time tree of nested node types. Nothing is computed until the programmer explicitly asks for a result by calling `eval()`. At that point a single recursive walk fuses the whole computation into one pass, with no intermediate temporaries exposed to the caller.

DSLtk provides this machinery through the `ExprTemplates` feature tag, the `ExprLike` concept, the `BinExpr` and `UnaryExpr` node templates, and a set of overloaded arithmetic operators. This chapter describes how the lazy tree is constructed. The mechanics of evaluation — `eval()`, `eval_operand()`, and fusion — are covered in detail in Chapter 16. The present chapter focuses on the construction side: how a DSL type becomes an expression leaf, how operators compose nodes, and what the resulting type encodes.

## The Problem: Eager Evaluation Loses Structure

Consider a DSL for vector arithmetic where each `VecDSL` object stands for a symbolic vector of known length. If `operator+` for two `VecDSL` objects returned a new `VecDSL` holding the elementwise sum, then an expression like `a + b * c + a` would allocate two or three intermediate vectors and discard them. The final result would be correct, but every intermediate would have been materialized, copied, and thrown away.

Worse, any later analysis — constant folding, common-subexpression elimination, code generation — would have nothing to work with. The intermediate vectors are opaque values; they do not remember that they were once a sum or a product. To recover the structure, the library would have to re-parse the expression from a string, which defeats the purpose of an embedded DSL.

The expression-template technique preserves the structure by giving each operator a return type that *is* the structure. A sum is not a value, it is a `BinExpr<std::plus<>, A, B>`. The type carries the operation tag and the types of both operands; the value carries references to the operands themselves. The tree exists in the program's type system, where the compiler can see it, and in the program's runtime object graph, where the evaluator can walk it.

## What an Expression Template Is

An expression template is a compile-time representation of an arithmetic expression as a tree of operation nodes. Each node has a type that names the operation (`std::plus<>`, `std::multiplies<>`, `std::negate<>`) and the types of its operands. Leaves of the tree are the user's DSL values; internal nodes are `BinExpr` or `UnaryExpr` instances.

The defining property is laziness. Constructing a node performs no arithmetic. The node merely stores (copies of, or references to) its operands and remembers which operation to apply. Evaluation is deferred until an explicit `eval()` call traverses the tree and performs the computation. Because the tree is built entirely at compile time through template instantiation, the compiler can often inline the entire evaluation down to the same machine code an eager expression would produce, but without exposing any intermediate to the caller.

This is the same technique used by Blitz++, Boost.uBLAS, and Eigen. DSLtk provides a small, self-contained implementation intended for embedded DSLs whose leaves are user-defined domain types.

## The ExprTemplates Feature Tag

The `ExprTemplates` feature tag is the entry point. Like every DSLtk feature tag (see Chapter 5), it is an empty struct with a nested `Mixin` template that injects members into the CRTP base class `dsl::DSL<Derived, Features...>` (see Chapter 3). A DSL type opts into expression templates by listing `dsl::ExprTemplates` among its feature arguments.

```cpp
struct ExprTemplates
{
  template <typename Derived> struct Mixin
  {
    /// operator+ (binary)
    template <typename R>
    friend constexpr auto
    operator+ (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::plus<>, Derived, R>{ lhs, rhs };
    }

    /// operator- (binary)
    template <typename R>
    friend constexpr auto
    operator- (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::minus<>, Derived, R>{ lhs, rhs };
    }

    /// operator* (binary)
    template <typename R>
    friend constexpr auto
    operator* (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::multiplies<>, Derived, R>{ lhs, rhs };
    }

    /// operator/ (binary)
    template <typename R>
    friend constexpr auto
    operator/ (const Derived &lhs, const R &rhs)
    {
      return BinExpr<std::divides<>, Derived, R>{ lhs, rhs };
    }

    /// Unary negation
    friend constexpr auto
    operator- (const Derived &e)
    {
      return UnaryExpr<std::negate<>, Derived>{ e };
    }
  };
};
```

The `Mixin` declares five `friend` functions, found through argument-dependent lookup whenever at least one operand has type `Derived`. Four of them are the binary arithmetic operators `+`, `-`, `*`, `/`; the fifth is unary negation. Each returns a freshly constructed node rather than a computed value. The return type names `Derived` as the left operand, so the node type is parameterized by the concrete DSL type — for example `BinExpr<std::plus<>, Scalar, Scalar>` when both operands are `Scalar`.

Because these operators are templates on the right-hand operand `R`, the right operand need not have the same type as the left. A `Scalar` may be added to a `BinExpr`, or to a `UnaryExpr`, or even to a plain `int`. The only requirement is that the eventual evaluation can resolve every operand to a value, which the `eval_operand` machinery in Chapter 16 handles via `if constexpr` dispatch.

## The ExprLike Concept

A type is not useful as an expression leaf unless the evaluator can extract a value from it. DSLtk captures this requirement with the `ExprLike` concept, defined in the utility-concepts section of the header (see Chapter 5).

```cpp
template <typename T>
concept ExprLike = HasFeature<T, ExprTemplates>
                   && requires (const T &t) { t.expr_value (); };
```

Two conditions must hold. First, `T` must mix in the `ExprTemplates` feature, which `HasFeature` checks by testing whether `T` is derived from `ExprTemplates::Mixin<T>`. Second, `T` must expose a `const`-qualified `expr_value()` member that returns the underlying value. Both conditions are structural; the concept verifies the *shape* of the type, not the meaning of its value.

The `expr_value()` accessor is the contract a leaf type fulfills. Whatever it returns — a `double`, a vector, an AST node, a parser result — is what the evaluator will feed into the operation tag. By keeping the return type unconstrained, DSLtk leaves the value domain entirely to the user. The same machinery serves a scalar calculator, a symbolic vector DSL, and a parser-cost estimator; only the leaf type and its `expr_value()` change.

Note that `ExprLike` is not used to constrain the operator templates in the `ExprTemplates` mixin. Those operators accept any `Derived` on the left (they are found via ADL on `Derived`) and any `R` on the right. `ExprLike` is instead available for users who wish to write their own generic algorithms over expression leaves, or to constrain their own DSL extension points. The concept is descriptive of a leaf type, not prescriptive of the operator set.

## A Minimal Leaf: The Scalar Type

The simplest concrete leaf is a scalar wrapper around a `double`. It derives from `dsl::DSL<Scalar, dsl::ExprTemplates>` to inherit the operator mix-in, stores a `double`, and exposes `expr_value()` to return it.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

int main() {
  Scalar a{2}, b{3}, c{4};
  auto expr = a + b * c - a;
  std::cout << expr.eval() << "\n";   // 14
}
```

This is the example shipped as `examples/12-expression-templates.cpp`. The line `auto expr = a + b * c - a;` constructs a lazy tree and binds it to `expr` without performing any arithmetic. The subsequent `expr.eval()` walks the tree and prints `14`, since `2 + 3*4 - 2 = 14`.

Observe the use of `auto`. The type of `expr` is a deeply nested `BinExpr` instantiation; spelling it out by hand is impractical and unnecessary. The chapter returns to this point in the section on pitfalls below.

## The BinExpr Node

Each binary operator produces a `BinExpr`. The node template is parameterized by an operation tag, a left-operand type, and a right-operand type. The node stores one instance of each operand, taken by value in the constructor.

```cpp
template <typename Op, typename L, typename R> struct BinExpr
{
  L lhs;
  R rhs;

  constexpr BinExpr (L l, R r) : lhs (std::move (l)), rhs (std::move (r)) {}

  constexpr auto
  eval () const
  {
    auto l = eval_operand (lhs);
    auto r = eval_operand (rhs);
    return Op{}(l, r);
  }
  // ...
};
```

The two data members `lhs` and `rhs` hold the operands *by value*. When the left operand is a `Scalar`, the node contains a copy of that `Scalar`. When the left operand is itself a `BinExpr`, the node contains a copy of that inner node, which in turn contains copies of its operands. The tree is a value: copying the root copies the entire structure.

Storing operands by value is the design choice that makes expressions safe to return from functions and store in variables. A node never dangles a reference at a stack variable that has gone out of scope, because it does not hold references at all — it holds copies. (The trade-off is copying cost, which the chapter returns to below.) The `std::move` in the constructor allows callers to move rvalue operands into the node rather than copying.

The `eval()` member is `constexpr`, so a fully constructed expression can be evaluated at compile time when its operands are literal types. The example above could in principle be made `constexpr` and evaluated in a `static_assert`. The `eval_operand` helper, described in Chapter 16, dispatches on whether the operand is itself evaluable (a node), an `ExprLike` leaf, or a plain value.

## The UnaryExpr Node

Unary operations produce a `UnaryExpr`, which stores a single operand. The only unary operation currently provided is negation, which uses `std::negate<>` as its operation tag.

```cpp
template <typename Op, typename E> struct UnaryExpr
{
  E operand;

  explicit constexpr UnaryExpr (E e) : operand (std::move (e)) {}

  constexpr auto
  eval () const
  {
    if constexpr (requires { operand.eval (); })
      return Op{}(operand.eval ());
    else if constexpr (requires { operand.expr_value (); })
      return Op{}(operand.expr_value ());
    else
      return Op{}(operand);
  }
};
```

`UnaryExpr` mirrors `BinExpr`: a tag, an operand, a `constexpr` constructor, and an `eval()` that recursively resolves its operand and applies the operation. The dispatch logic is duplicated inline rather than factored through `eval_operand`, but the effect is the same — a node can be negated, a leaf can be negated, and a plain value can be negated.

Applying unary negation to a `Scalar` produces a `UnaryExpr<std::negate<>, Scalar>`. Applying it to a `BinExpr` produces a `UnaryExpr<std::negate<>, BinExpr<...>>`, nesting the existing tree inside the negation. Because the operator returns by value, the resulting node owns its subtree completely.

## How the Operators Compose

The arithmetic operators are the construction site of the lazy tree. Each operator call instantiates one node and returns it. Composition arises because the return type of one operator becomes the operand type of the next.

Consider the expression `b * c`. Both operands are `Scalar`. The `ExprTemplates` mixin's `operator*` is found via ADL on the left operand and instantiated with `Derived = Scalar` and `R = Scalar`. It returns `BinExpr<std::multiplies<>, Scalar, Scalar>`, constructed from copies of `b` and `c`. No multiplication has occurred; the node merely holds the two scalars and the tag `std::multiplies<>`.

Now consider `a + b * c`. The right operand of `+` is the `BinExpr` just constructed. The mixin's `operator+` is instantiated with `Derived = Scalar` and `R = BinExpr<std::multiplies<>, Scalar, Scalar>`. It returns `BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>`. The type literally reads as "the sum of a `Scalar` and the product of two `Scalar`s". The structure of the expression is encoded in the type.

Finally, `a + b * c - a` subtracts `a` from the previous result. Here the left operand is a `BinExpr`, not a `Scalar`. The mixin's `operator-` is found via ADL on `Derived = Scalar`, so it cannot apply when the left operand is a `BinExpr`. DSLtk therefore provides a second set of free-function overloads that handle a `BinExpr` on the left.

```cpp
template <typename Op1, typename L1, typename R1, typename R>
constexpr auto
operator+ (const BinExpr<Op1, L1, R1> &lhs, const R &rhs)
{
  return BinExpr<std::plus<>, BinExpr<Op1, L1, R1>, R>{ lhs, rhs };
}

template <typename Op1, typename L1, typename R1, typename R>
constexpr auto
operator- (const BinExpr<Op1, L1, R1> &lhs, const R &rhs)
{
  return BinExpr<std::minus<>, BinExpr<Op1, L1, R1>, R>{ lhs, rhs };
}

template <typename Op1, typename L1, typename R1, typename R>
constexpr auto
operator* (const BinExpr<Op1, L1, R1> &lhs, const R &rhs)
{
  return BinExpr<std::multiplies<>, BinExpr<Op1, L1, R1>, R>{ lhs, rhs };
}

template <typename Op1, typename L1, typename R1, typename R>
constexpr auto
operator/ (const BinExpr<Op1, L1, R1> &lhs, const R &rhs)
{
  return BinExpr<std::divides<>, BinExpr<Op1, L1, R1>, R>{ lhs, rhs };
}
```

These four overloads accept any `BinExpr` as the left operand and any type `R` as the right. They wrap the pair in a fresh `BinExpr` whose left type is the inner `BinExpr`. The subtraction in `a + b * c - a` therefore produces `BinExpr<std::minus<>, BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>, Scalar>` — a left-leaning tree whose root is a minus, whose left child is the prior sum, and whose right child is the leaf `a`.

The tree is left-leaning because all binary operators in C++ associate left-to-right. The expression `a - b - c` parses as `(a - b) - c`, and the resulting node type reflects that grouping. For commutative operations like addition and multiplication the grouping is mathematically irrelevant; for non-commutative operations like subtraction and division it determines the order of evaluation. Programmers who need a different grouping must parenthesize explicitly, which produces the corresponding nested node type.

## Building a Tree Step by Step

It is instructive to watch a tree grow one operator at a time. Start with three leaves.

```cpp
Scalar a{2}, b{3}, c{4};
```

The first operation, `b * c`, yields a node of type `BinExpr<std::multiplies<>, Scalar, Scalar>`. The node's two members are copies of `b` and `c`; the operation tag is the empty type `std::multiplies<>`. No arithmetic has been performed.

```cpp
auto bc = b * c;            // BinExpr<multiplies<>, Scalar, Scalar>
```

The next operation, `a + bc`, invokes the mixin's `operator+` with `Derived = Scalar` and `R = decltype(bc)`. The result is a deeper node.

```cpp
auto abc = a + bc;          // BinExpr<plus<>, Scalar, BinExpr<multiplies<>, Scalar, Scalar>>
```

The final operation, `abc - a`, invokes the free-function `operator-` overload for a `BinExpr` left operand. The result is deeper still.

```cpp
auto expr = abc - a;
// BinExpr<minus<>,
//   BinExpr<plus<>, Scalar, BinExpr<multiplies<>, Scalar, Scalar>>,
//   Scalar>
```

Each binding stores a complete value of the named type. The leaves `a`, `b`, `c` are copied into the innermost nodes; the inner nodes are copied into their parents; the root `expr` owns the entire structure. Evaluating `expr.eval()` walks from the root down to the leaves, applying each operation tag in post-order. Chapter 16 traces this walk in detail.

## The Type Encodes the Shape

The most important property of an expression-template type is that it records the shape of the expression. The type `BinExpr<std::plus<>, Scalar, BinExpr<std::multiplies<>, Scalar, Scalar>>` is, in effect, a small abstract syntax tree printed in template syntax. The operation tag at each position names the node, and the operand types name the children.

This property has several consequences. First, two expressions that compute the same value but have different shapes have different types. `a + b * c` and `a + c * b` are not the same type, even when `b` and `c` are equal at runtime, because the operand order in the multiplication differs. The compiler treats them as distinct types and will not, for example, allow one to be assigned to the other.

Second, the type is available for compile-time introspection. A generic algorithm can constrain on `BinExpr` instantiations, inspect the operation tag, or recurse into the operand types. Template metaprogramming over expression types is possible, though DSLtk does not impose any particular metaprogramming interface; the node templates are intentionally simple structs whose members are public.

Third, the type is usually too complex to write by hand. A modest expression of six operators can easily produce a type name over a hundred characters long. The idiomatic way to bind an expression is `auto`, which deduces the full type without requiring the programmer to spell it. Where the type must be named — for example, as a function parameter or a class member — a trailing return type or a type alias is used.

## A Vector Leaf: Beyond Scalars

The `Scalar` example is the simplest possible leaf, but expression templates are not limited to scalars. Any type that mixes in `ExprTemplates` and exposes `expr_value()` participates in the same machinery. Consider a small vector DSL whose leaves stand for elementwise vectors.

```cpp
#include "DSLtk.hpp"
#include <array>
#include <iostream>

struct VecDSL : dsl::DSL<VecDSL, dsl::ExprTemplates> {
  std::array<double, 3> v{};
  VecDSL() = default;
  VecDSL(double x, double y, double z) : v{x, y, z} {}
  std::array<double, 3> expr_value() const { return v; }
};

std::array<double, 3> operator+(std::array<double, 3> a, std::array<double, 3> b) {
  return {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}
std::array<double, 3> operator*(std::array<double, 3> a, std::array<double, 3> b) {
  return {a[0]*b[0], a[1]*b[1], a[2]*b[2]};
}

int main() {
  VecDSL a{1, 2, 3}, b{4, 5, 6}, c{7, 8, 9};
  auto expr = a + b * c;            // lazy tree
  auto r = expr.eval();             // single fused pass
  std::cout << r[0] << ' ' << r[1] << ' ' << r[2] << "\n";  // 29 42 57
}
```

The `VecDSL` leaf returns a `std::array<double, 3>` from `expr_value()`. The `BinExpr` `eval()` applies `std::plus<>` and `std::multiplies<>` to those arrays, which in turn requires overloads of `operator+` and `operator*` for `std::array`. The point is that the leaf type completely determines the value domain; the expression-template machinery is value-agnostic.

Because evaluation is fused, the expression `a + b * c` allocates no intermediate arrays visible to the caller. The `eval_operand` calls resolve each leaf to an array, and the operation tags produce the final array in a single composition. For larger vectors or more complex expressions, this is the central benefit of the technique.

## Storing an Expression for Later Evaluation

A common pattern is to build an expression in one part of a program and evaluate it in another. Because the node types are regular value types, an expression can be stored in a variable, returned from a function, or held in a container.

```cpp
auto make_expr(Scalar a, Scalar b, Scalar c) {
  return a + b * c - a;
}

Scalar a{2}, b{3}, c{4};
auto e = make_expr(a, b, c);   // type encodes the whole expression
// ... later, perhaps in a different scope ...
double result = e.eval();
```

The function `make_expr` returns a deeply nested `BinExpr` by value. Move semantics make the return cheap: the inner nodes are moved into the return value rather than copied. The caller binds the result with `auto` and evaluates it later. Between construction and evaluation the expression is a pure value carrying the structure of the computation.

This pattern is the basis of deferred query languages, symbolic calculators, and code generators. The expression is a first-class object: it can be passed to functions, stored in data structures, serialized (with appropriate support), and evaluated zero, one, or many times. Each `eval()` call walks the tree afresh; the tree itself is not consumed.

## Repeated Evaluation

Because `eval()` is a `const` member that does not mutate the tree, an expression can be evaluated repeatedly. This is useful when the leaves are mutable: change a leaf, re-evaluate, observe a new result without rebuilding the tree.

There is a subtlety. The nodes store their operands by value, so the tree holds *copies* of the leaves, not references to them. Changing the original `Scalar a` after the tree is built does not affect the copy inside the tree. To observe changes, the leaf type itself must be made to share state — for example by holding its value through a `std::shared_ptr` — or the expression must be rebuilt after each change.

For most DSL applications this is acceptable. Expressions are built, evaluated once, and discarded. Where repeated evaluation against changing leaves is required, the leaf type can be designed to expose a level of indirection, but that is a property of the leaf, not of the expression-template machinery.

## Constexpr Potential

Every node constructor and every `eval()` member is declared `constexpr`. This means an expression whose leaves are literal types can be constructed and evaluated at compile time, producing a constant result with no runtime cost.

```cpp
constexpr double square_plus_one(double x) {
  Scalar s{x};
  Scalar one{1.0};
  auto expr = s * s + one;
  return expr.eval();
}

static_assert(square_plus_one(3.0) == 10.0);
```

For this to compile, the leaf type must itself be a literal type: its constructor and `expr_value()` must be `constexpr`, and it must have no non-literal members. The `Scalar` type shown earlier satisfies this with its defaulted default constructor, its `constexpr` value constructor, and its `constexpr` `expr_value()`. The `VecDSL` type using `std::array` is also a literal type, since `std::array` of literal types is itself literal.

Compile-time evaluation is not the primary motivation for expression templates, but it is a consequence of the value-based, `constexpr`-friendly design. Where a computation can be hoisted to compile time, the machinery allows it without changes to the expression syntax.

## Leaves With Mixed Operand Types

The operator templates accept any type `R` as the right operand. This permits expressions where the two operands of a binary operation have different types — for example a `Scalar` plus a plain `double`, or a `Scalar` plus a `BinExpr`.

```cpp
Scalar a{2}, b{3};
auto e1 = a + 5.0;                  // BinExpr<plus<>, Scalar, double>
auto e2 = a + b * 2.0;             // BinExpr<plus<>, Scalar, BinExpr<multiplies<>, Scalar, double>>
```

Evaluation resolves each operand through `eval_operand`. A `Scalar` resolves via `expr_value()`; a `double` resolves directly, since it has neither `eval()` nor `expr_value()`. The operation tag `std::plus<>` is then applied to the resulting pair. As long as the operation is defined for the resolved value types, the expression is well-formed.

This flexibility is valuable when a DSL must interoperate with raw numeric types. A user can write `a * 2.0 + b` without wrapping the literal `2.0` in a `Scalar`, and the resulting tree still fuses to a single evaluation pass. The cost is that the type of `e1` mentions `double` as an operand, which can complicate generic code that assumes uniform operand types.

## The Friend Operator Pattern

The binary operators live inside the `ExprTemplates::Mixin` as `friend` function templates. This is a deliberate C++ idiom. A `friend` function defined inside a class template is only instantiated when found via argument-dependent lookup, and only for the specific template arguments that match a call. The effect is that each DSL type `Derived` gets its own set of operators, found only when an operand of type `Derived` appears.

Without the `friend` trick, the operators would be ordinary free function templates visible in the enclosing namespace. They would then participate in overload resolution for every arithmetic expression in the program, including those that have nothing to do with DSLtk, producing ambiguity errors and surprising diagnostics. By embedding the operators in the mixin as `friend`s, DSLtk ensures they are considered only for expressions involving a `Derived` type.

The free-function overloads for `BinExpr` left operands are ordinary templates in the `dsl` namespace. They are found via ADL on the `BinExpr` argument, which lives in `dsl`. Because `BinExpr` is exclusively a DSLtk type, these overloads do not pollute the overload sets of unrelated arithmetic.

## Why Laziness Matters: Fusion

The central performance argument for expression templates is fusion. An eager evaluation of `a + b * c` over vectors allocates one temporary for the product, another for the sum, and discards both. A fused evaluation walks the tree once, reading each leaf, performing the operations in post-order, and writing the final result directly. No intermediate is materialized.

For a single expression of two operators the saving is modest. For a chain of ten operators over large vectors, the saving is substantial: ten allocations become one, and the data passes through cache once instead of ten times. The tree walk is recursive, but the recursion depth equals the operator count, which is small and bounded by the source expression.

Fusion is the topic of Chapter 16, which traces `eval()` and `eval_operand` in detail. The present chapter's concern is the construction side: ensuring that the tree exists, that its type encodes the operation, and that the operands are safely captured. Construction is the prerequisite for fusion; without a tree there is nothing to fuse.

## Composition With Other Features

`ExprTemplates` is one feature among many. A DSL type can mix in several features simultaneously, and the expression-template operators coexist with the pipeline, operator, rewrite, and other feature mixins described in Chapters 4 through 14. The CRTP base `dsl::DSL<Derived, Features...>` linearizes each feature's mixin, so the operators from `ExprTemplates` appear alongside the predicate operators from `Operators` and the `rewrite` method from `Rewrite`.

Care is needed when feature sets overlap. The `Operators` feature (Chapter 7) also provides `operator&&`, `operator||`, and `operator!` for predicate composition. These operate on a different value domain — boolean predicates — and do not collide with the arithmetic operators of `ExprTemplates`. A DSL that mixes both features gains both operator sets, and the compiler selects between them by argument type.

A DSL type that wishes to expose both symbolic arithmetic and predicate composition would therefore list both features:

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::ExprTemplates, dsl::Operators> {
  double v{};
  double expr_value() const { return v; }
  bool operator()(int x) const { return x > 0; }
};
```

The resulting type can participate in arithmetic expression trees *and* in predicate composition. The two domains are kept separate by the types of the values involved: `expr_value()` returns the arithmetic value, while `operator()` returns the predicate value. This separation is a consequence of the value-agnostic design of the node templates.

## A Larger Worked Example

To consolidate the material, consider a slightly larger expression that exercises every operator and a unary negation.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

int main() {
  Scalar a{10}, b{3}, c{2}, d{5};
  auto expr = a + b * c - d / a + (-b);
  std::cout << expr.eval() << "\n";   // 10 + 6 - 0.5 + (-3) = 12.5
}
```

The expression `a + b * c - d / a + (-b)` builds a tree with five binary nodes and one unary node. Reading the type from the inside out: `b * c` is a `BinExpr<std::multiplies<>, Scalar, Scalar>`; `d / a` is a `BinExpr<std::divides<>, Scalar, Scalar>`; `-b` is a `UnaryExpr<std::negate<>, Scalar>`. The additions and subtraction then nest these as operands, producing a left-leaning tree whose root is the final `+`.

Evaluation walks the tree once. Each leaf resolves to a `double` via `expr_value()`, each inner node applies its operation tag, and the final result is `12.5`. No intermediate `double` is exposed to the caller; the entire computation is fused into the single `eval()` call.

## Pitfall: Temporaries and Lifetime

The most common error with expression templates is building an expression that references a temporary. Because `BinExpr` stores its operands by value, this is less hazardous than with reference-based expression templates, but the related pitfall remains when the leaf type itself holds references or pointers.

Consider a leaf that wraps a reference to an external container. If the container is a temporary, the leaf dangles after the temporary is destroyed, and the expression that contains the leaf dangles with it. The expression-template machinery cannot prevent this; it can only ensure that the nodes themselves own their immediate operands. The leaf type is responsible for its own lifetime invariants.

A related concern arises when an expression is stored in a variable and the leaves it was built from are later destroyed. Because the nodes copy their operands, the expression survives the destruction of the originals — *provided* the leaf type's copy is a deep copy. A leaf that holds a `std::vector` by value copies cleanly; a leaf that holds a `std::vector&` does not. The rule of thumb is the same as for any value type: leaves should be value types, or should manage shared state explicitly.

```cpp
Scalar make_temp() { return Scalar{42}; }
auto bad = make_temp() + Scalar{1};   // OK: nodes hold copies, not references
```

In the snippet above the expression is safe, because `BinExpr` stores its operands by value and `Scalar` is a value type. The temporary `Scalar{42}` is moved into the node's left member, and the temporary `Scalar{1}` is moved into the right member; both temporaries are then destroyed, but their contents live on inside the node. This is the principal advantage of value-based storage over reference-based storage.

## Pitfall: The Type Is Complex

The type of an expression is a nesting of `BinExpr` and `UnaryExpr` instantiations that mirrors the source expression. For anything beyond a few operators, the type is too long to write conveniently and too specific to anticipate. The idiomatic remedy is `auto`.

```cpp
auto e = a + b * c;        // good: type deduced
// BinExpr<plus<>, Scalar, BinExpr<multiplies<>, Scalar, Scalar>> e2 = a + b * c;
//                                       ^ impractical to write
```

Where the type must be named — as a function return type, a class member, or a template parameter — use `decltype` or a trailing return type. A function that builds and returns an expression can declare its return type as `decltype(a + b * c)`:

```cpp
auto make_expr(const Scalar& a, const Scalar& b, const Scalar& c) {
  return a + b * c;
}
```

Here the return type is deduced from the `return` statement, and the caller uses `auto` to bind the result. This style keeps the source readable while preserving the full type information for the compiler.

Type erasure is sometimes appropriate when an expression must cross an API boundary that cannot expose the full type. A `std::function<double()>` wrapping `[e]{ return e.eval(); }` erases the expression type at the cost of a heap allocation and an indirect call. DSLtk does not provide such an erasure itself, but the regular value semantics of the node types make it straightforward to add.

## Pitfall: Assignment Does Not Rebuild the Tree

Because each expression has a distinct type, an expression variable cannot be reassigned to a different shape. `auto e = a + b;` gives `e` the type `BinExpr<std::plus<>, Scalar, Scalar>`; a later `e = a * b;` is ill-formed because `a * b` has type `BinExpr<std::multiplies<>, Scalar, Scalar>`. The two types are unrelated despite their structural similarity.

When a program needs to vary the shape of an expression at runtime, the usual approach is to select among pre-built expressions of the same type, or to erase the type as described above. Alternatively, the leaf type can be made to carry a runtime tag that selects the operation, and a single fixed expression shape can evaluate to different results based on that tag. These are workarounds for the fact that expression templates are fundamentally a compile-time technique.

## Pitfall: Overload Ambiguity

The free-function overloads for `BinExpr` left operands accept any `R` as the right operand. When `R` is itself a `BinExpr`, the result is a deeper tree, which is usually what is wanted. When `R` is a type that also has its own arithmetic operators — for example a type from another library — overload resolution may become ambiguous.

The remedy is to ensure that the right operand is either a DSLtk leaf, a DSLtk node, or a plain value type that does not have competing `operator+` overloads found via ADL. Mixing expression-template types from different libraries in a single expression is generally not supported without explicit glue. Within a single DSLtk-based program, the operators are designed to compose without ambiguity.

## Inspecting a Tree at Compile Time

Although DSLtk does not provide a dedicated metaprogramming interface for expression trees, the node types are simple enough to introspect directly. A generic trait can detect `BinExpr` specializations, extract their operation tag and operand types, and recurse.

```cpp
template <typename T> struct ExprShape;
template <typename Op, typename L, typename R>
struct ExprShape<dsl::BinExpr<Op, L, R>> {
  using op = Op;
  using left = L;
  using right = R;
};
```

Such a trait can be used to write algorithms that walk the type at compile time — for example, to verify that an expression contains no division, or to count the number of operation nodes. The header does not impose any particular trait; users are free to define their own. The `ExprTerminal` tag type is provided as a marker for terminal nodes should a metaprogram wish to distinguish them, though the leaf types themselves are not required to derive from it.

This kind of introspection is rarely needed in application code, but it is the foundation of more advanced techniques such as automatic differentiation or symbolic simplification built on top of DSLtk expression trees.

## Relationship to Chapter 16

This chapter has concentrated on tree construction. The companion Chapter 16 covers the other half of the design: the `eval()` member functions, the `eval_operand` dispatch helper, and the fusion behavior that makes a single walk replace a chain of eager computations. Readers who have understood the construction side will find the evaluation side straightforward; the tree is already in place, and evaluation is a recursive walk that applies each operation tag in post-order.

The two chapters are deliberately split so that construction — the part visible to the DSL author who writes `a + b * c` — is separate from evaluation — the part visible to the runtime that calls `eval()`. Most users interact only with construction. The evaluation internals matter for understanding performance characteristics and for extending the machinery to new operation tags.

## Summary

Expression templates capture arithmetic expressions as compile-time trees of operation nodes. The `ExprTemplates` feature tag injects arithmetic operators into a DSL type via its CRTP mixin; the operators return `BinExpr` and `UnaryExpr` nodes rather than computed values. The `ExprLike` concept documents the contract a leaf type must fulfill: mix in `ExprTemplates` and expose `expr_value()`. The node types store their operands by value, which makes expressions safe to store, return, and copy without lifetime concerns, at the cost of copying the leaves into the tree. The type of an expression encodes its shape, making `auto` the idiomatic binding for expression variables. Because no arithmetic is performed at construction time, a later `eval()` can fuse the entire computation into a single pass — the subject of Chapter 16. The same machinery serves scalars, vectors, and any user-defined value type, and the `constexpr` design permits compile-time construction and evaluation when the leaf types are literal types. The principal pitfalls are the complexity of the resulting types, the value-semantics requirement on leaves, and the impossibility of reassigning an expression to a different shape without type erasure.
