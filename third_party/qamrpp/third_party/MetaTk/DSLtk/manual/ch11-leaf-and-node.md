# Chapter 11: Building Trees with `leaf<>` and `node<>`

Chapter 10 introduced `ASTNode`, the value-type node that backs every abstract syntax tree in DSLtk. Constructing those nodes by hand, however, requires spelling out the tag as a run-time `std::string_view`, choosing the correct constructor overload, and assembling the children vector yourself. The library therefore provides two ergonomic factory functions, `dsl::leaf<>` and `dsl::node<>`, that turn the common cases into one-line expressions. This chapter documents those factories in detail.

Both factories share a single design idea: the tag is supplied as a compile-time string literal in the template argument list, while the run-time payload (a value or a list of children) is supplied as ordinary function arguments. The result is an `ASTNode` prvalue that can be immediately moved into a parent or bound to a variable. The factories are the primary way DSLtk code builds trees, and every later chapter on traversal, rewriting, and lazy evaluation assumes the trees were assembled this way.

## 11.1 The Role of the Factories

An `ASTNode` is, mechanically, a small class with a tag string, a value string, a vector of children, and a boolean distinguishing leaves from inner nodes. Its constructors accept `(tag, value)` for leaves and `(tag, children)` for inner nodes. Nothing prevents a user from invoking those constructors directly, and doing so is sometimes useful in generic code.

The factories exist because the direct form is verbose and error-prone at call sites. The tag, which is almost always a literal known at compile time, has to be wrapped in a `std::string_view` or a `std::string` by hand. The children vector has to be constructed and moved explicitly. The intent of the code is buried under mechanics.

`dsl::leaf<Tag>(value)` and `dsl::node<Tag>(children...)` lift the tag into the template parameter list, where a string literal can be written directly, and handle the assembly internally. They are pure convenience functions with no hidden state. Their behavior is entirely described by the definitions quoted in the next two sections.

## 11.2 The `leaf<>` Factory

The `leaf` template constructs a leaf node. A leaf is a node with a tag and a value but no children. Its definition, verbatim from the header, is:

```cpp
template <FixedString Tag, typename T>
ASTNode
leaf (T &&val)
{
  std::ostringstream os;
  os << std::forward<T> (val);
  return ASTNode{ Tag.view (), os.str () };
}
```

The template has two parameters. The first, `Tag`, is a non-type template parameter of type `dsl::FixedString` (Chapter 4). It is deduced from a string literal written in the angle brackets. The second, `T`, is the type of the value being stored and is deduced from the function argument.

The body of the factory is simple. It feeds the value into an `ostringstream`, which converts it to its textual form using ordinary stream insertion. It then constructs an `ASTNode` via the leaf constructor `ASTNode(std::string_view tag, std::string value)`, passing `Tag.view()` for the tag and the resulting string for the value.

The use of `ostringstream` means any type with a stream insertion operator can be stored directly. Integers, floating-point numbers, booleans, and anything for which `operator<<` is defined will be accepted.

## 11.3 String Overloads of `leaf`

Because the general template routes every value through an `ostringstream`, the header also provides two overloads that skip the stream for the common case where the value is already text. They are:

```cpp
/// Overload for string_view values (avoids extra stream).
template <FixedString Tag>
ASTNode
leaf (std::string_view val)
{
  return ASTNode{ Tag.view (), std::string (val) };
}

/// Overload for const char* values.
template <FixedString Tag>
ASTNode
leaf (const char *val)
{
  return ASTNode{ Tag.view (), std::string (val) };
}
```

These overloads are exact matches for `std::string_view` and `const char *` arguments, so overload resolution prefers them over the general template. They construct the `std::string` value directly from the argument, avoiding the cost of formatting through a stream.

From the caller's perspective the distinction is invisible. The same syntax, `dsl::leaf<"name">(argument)`, is used regardless of the argument type. What changes is only the internal conversion path.

## 11.4 A First Leaf Example

The smallest useful tree is a single leaf. The header's own overview gives the canonical form:

```cpp
auto leaf_node = dsl::leaf<"name">(value);
```

Concretely, a numeric leaf is written as follows.

```cpp
auto n = dsl::leaf<"num">(42);
std::cout << n.dump() << "\n";   // prints (num 42)
```

The `dump` member function (Chapter 10) renders a leaf as `(tag value)`. Because `42` is an `int`, the general template is selected, the integer is streamed into the `ostringstream`, and the resulting string `"42"` is stored as the leaf's value.

The same syntax accepts a string literal, in which case the `const char *` overload is selected.

```cpp
auto id = dsl::leaf<"ident">("foo");
std::cout << id.dump() << "\n";  // prints (ident foo)
```

Either way, the produced node reports `is_leaf() == true`, has an empty `children()` vector, and carries its payload in `value()`.

## 11.5 The `node<>` Factory

The `node` template constructs an inner node. An inner node has a tag and a list of children but no value of its own. Its definition, verbatim from the header, is:

```cpp
template <FixedString Tag, typename... Children>
  requires (std::convertible_to<Children, ASTNode> && ...)
ASTNode
node (Children &&...children)
{
  std::vector<ASTNode> v;
  v.reserve (sizeof...(children));
  (v.emplace_back (std::forward<Children> (children)), ...);
  return ASTNode{ Tag.view (), std::move (v) };
}
```

This is a variadic template. The `Tag` parameter is, as with `leaf`, a `FixedString` non-type template parameter. The `Children...` pack captures the run-time arguments, which must each be convertible to `ASTNode`.

The body builds a `std::vector<ASTNode>` of the right capacity, then uses a fold expression to perfect-forward each child into the vector via `emplace_back`. Finally it calls the inner-node constructor `ASTNode(std::string_view tag, std::vector<ASTNode> children)`, moving the assembled vector into the new node.

The `requires` clause is the only constraint. Every argument type must satisfy `std::convertible_to<Children, ASTNode>`. This permits `ASTNode` prvalues (the usual case) as well as anything that has a conversion to `ASTNode`.

## 11.6 The Tag as a Non-Type Template Parameter

The most important design choice in both factories is that the tag is a template argument, not a function argument. This is what allows the call site to write the tag as a bare string literal inside the angle brackets.

```cpp
dsl::node<"+">(lhs, rhs);
```

Here `"+"` is not an expression evaluated at run time. It is a template argument, and the compiler deduces a `FixedString` value from it. The `FixedString` class (Chapter 4) is a literal type with a fixed maximum length, designed precisely so that it can be used as a non-type template parameter.

Inside the factory, `Tag.view()` returns a `std::string_view` over the fixed buffer, which the `ASTNode` constructor copies into its own `std::string` member. The tag therefore lives in two places: a compile-time `FixedString` embedded in the type of the factory invocation, and a run-time `std::string` owned by the resulting node.

This split is what makes the factories both type-safe and convenient. The compiler verifies the tag at instantiation time, while the node retains a plain `std::string` for run-time inspection and comparison.

## 11.7 Why the Tag Cannot Be a Run-Time String

Because the tag is a non-type template parameter, it must be a constant expression. A string literal qualifies. A `const char *` variable does not, even if it points at a literal, because its value is not part of the type system.

```cpp
// OK: literal in the template argument list
auto a = dsl::leaf<"num">(1);

// Ill-formed: 'name' is not a constant expression
const char* name = "num";
auto b = dsl::leaf<name>(1);   // error
```

The same restriction applies to `std::string` objects, which are not literal types and therefore cannot be non-type template parameters at all. Code that needs to choose a tag at run time must fall back to constructing an `ASTNode` directly, passing the tag to the constructor.

This is an intentional trade-off. The factory syntax is optimized for the common case where the tag is a fixed, known label such as `"num"`, `"add"`, or `"if"`. Tags chosen at run time are rare in DSLtk usage, and the direct constructor remains available for them.

## 11.8 Children Must Be Convertible to `ASTNode`

The `node` factory places no constraint on the tag, but it constrains its arguments. The `requires` clause demands that every child type satisfy `std::convertible_to<Children, ASTNode>`. In practice this means each argument must be an `ASTNode` or something that converts to one.

The most common argument type is, of course, an `ASTNode` prvalue returned by another `leaf` or `node` call. The conversion is trivial in that case. An lvalue `ASTNode` also works; it will be copy-constructed into the vector.

```cpp
auto lhs = dsl::leaf<"num">(1);
auto rhs = dsl::leaf<"num">(2);
auto add = dsl::node<"+">(lhs, rhs);   // copies lhs and rhs
```

Types that are not convertible to `ASTNode` are rejected at compile time. There is no implicit conversion from, say, `int` to `ASTNode`, so passing a bare integer as a child is an error.

```cpp
auto bad = dsl::node<"+">(1, 2);   // error: int is not convertible to ASTNode
```

The fix is to wrap each scalar in a `leaf` first. The factories do not guess how to promote a value to a node; the user states the tag explicitly.

## 11.9 A First Inner Node

The header's overview gives the canonical two-child form:

```cpp
auto inner_node = dsl::node<"op">(child1, child2);
```

Combined with two leaves, this produces the smallest non-trivial tree.

```cpp
auto expr = dsl::node<"add">(
    dsl::leaf<"num">(1),
    dsl::leaf<"num">(2));
std::cout << expr.dump() << "\n";   // (add (num 1) (num 2))
```

This is the exact example from the header's AST overview. The resulting node has `is_leaf() == false`, `tag() == "add"`, `size() == 2`, and `child(0)` and `child(1)` are the two numeric leaves.

The order of the children is preserved exactly as written. The `emplace_back` fold expression processes the pack left to right, so the first argument becomes `child(0)`, the second becomes `child(1)`, and so on. Ordering is significant for most ASTs and is guaranteed.

## 11.10 Variadic Arity

The `node` factory accepts any number of children, including zero. Each additional child is simply another argument in the pack.

```cpp
auto unary  = dsl::node<"neg">(dsl::leaf<"num">(4));
auto binary = dsl::node<"+">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
auto ternary = dsl::node<"if">(
    dsl::leaf<"bool">(true),
    dsl::leaf<"num">(1),
    dsl::leaf<"num">(2));
```

A node with zero children is also valid. It is constructed as an inner node with an empty vector, distinct from a leaf.

```cpp
auto empty = dsl::node<"block">();
std::cout << empty.dump() << "\n";   // (block)
std::cout << empty.is_leaf() << "\n"; // 0
```

This form is occasionally useful for placeholder nodes such as empty statement blocks, where the absence of children is meaningful but the node is still structurally an inner node.

## 11.11 Composing Trees by Nesting

Because both factories return `ASTNode` prvalues, they compose by nesting. A `node` call can take further `node` calls as arguments, which in turn can take `leaf` or `node` calls, to arbitrary depth.

```cpp
auto t = dsl::node<"+">(
    dsl::node<"*">(
        dsl::leaf<"ident">("a"),
        dsl::leaf<"ident">("b")),
    dsl::leaf<"ident">("c"));
std::cout << t.dump() << "\n";
// (+ (* (ident a) (ident b)) (ident c))
```

The whole tree is built in a single expression. No temporary variables are required, though they may be used for readability. The `dump` output is a flat S-expression that preserves the nesting.

This compositional style is the idiomatic way to build ASTs in DSLtk. It mirrors the structure of the source language directly: an expression is a node whose children are subexpressions, all the way down to identifiers and literals.

## 11.12 Move Semantics and Prvalues

The factories return `ASTNode` by value. When a factory call appears as an argument to another factory, the inner prvalue is materialized and then moved (or copy-elided) into the parent's vector. The `emplace_back` call in `node` takes a forwarding reference, so an rvalue argument is moved into the vector rather than copied.

```cpp
auto t = dsl::node<"+">(
    dsl::leaf<"num">(1),   // prvalue, moved into the vector
    dsl::leaf<"num">(2));  // prvalue, moved into the vector
```

For lvalue arguments the situation is different. An lvalue `ASTNode` is copy-constructed into the vector, because `emplace_back` on an lvalue reference invokes the copy constructor.

```cpp
auto a = dsl::leaf<"num">(1);
auto b = dsl::leaf<"num">(2);
auto t = dsl::node<"+">(a, b);   // a and b are copied
// a and b are still valid here
```

Both forms are correct. The prvalue form avoids a copy; the lvalue form keeps the originals available for later use. DSLtk's value-semantics design (Chapter 10) means either choice is safe.

## 11.13 The `AST` Feature Tag

For DSLs that derive from `dsl::DSL`, the library provides an `AST` feature tag whose mixin exposes the same factories as static member functions. Its definition is:

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

The mixin simply forwards to the free functions. A DSL that mixes in `dsl::AST` gains `MyDSL::make_leaf<Tag>(...)` and `MyDSL::make_node<Tag>(...)` as aliases for the free functions.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::AST> {};
auto t = MyDSL::make_node<"root">(MyDSL::make_leaf<"val">(42));
```

The feature tag is purely a naming convenience. It collects the AST builders under the DSL's own scope, which can improve readability in larger programs that import several DSLs. The behavior is identical to calling `dsl::leaf` and `dsl::node` directly.

## 11.14 Building an Arithmetic Tree

The arithmetic expression `(+ (* a b) c)` is a classic two-level tree. With the factories it is written almost as it reads.

```cpp
auto expr = dsl::node<"+">(
    dsl::node<"*">(
        dsl::leaf<"ident">("a"),
        dsl::leaf<"ident">("b")),
    dsl::leaf<"ident">("c"));
std::cout << expr.dump() << "\n";
// (+ (* (ident a) (ident b)) (ident c))
```

The outer `+` node has two children. The first child is itself a `*` node with two identifier leaves. The second child is a single identifier leaf. The structure of the call mirrors the structure of the expression.

Tags here are short operator-like strings. There is nothing special about them; they are ordinary `FixedString` values chosen by convention. A DSL is free to use `"add"`, `"plus"`, or any other label, as long as the same label is used consistently by the code that inspects the tree.

## 11.15 Building an Assignment Tree

An assignment such as `x := x + 1` mixes a variable on the left and an expression on the right. The tree is naturally binary.

```cpp
auto assign = dsl::node<":=">(
    dsl::leaf<"ident">("x"),
    dsl::node<"+">(
        dsl::leaf<"ident">("x"),
        dsl::leaf<"num">(1)));
std::cout << assign.dump() << "\n";
// (:= (ident x) (+ (ident x) (num 1)))
```

The `:=` node carries no value of its own. Its meaning is entirely structural: the first child is the target, the second is the value to store. A later rewrite pass (Chapter 13) could match on the tag `":="` and transform the children accordingly.

The identifier `x` appears twice in this tree, once as a leaf on the left and once inside the addition. The two leaves are independent `ASTNode` objects that happen to carry the same text. There is no sharing of nodes; value semantics forbid it.

## 11.16 Building a Function Call Tree

A function call `(call f arg1 arg2)` has variable arity. The `node` factory handles this directly, since the children pack may be any length.

```cpp
auto call = dsl::node<"call">(
    dsl::leaf<"ident">("f"),
    dsl::leaf<"ident">("arg1"),
    dsl::leaf<"ident">("arg2"));
std::cout << call.dump() << "\n";
// (call (ident f) (ident arg1) (ident arg2))
```

By convention the first child is the function being called and the remaining children are the arguments. The factory itself enforces no such convention; it merely preserves order. The interpretation is up to the code that consumes the tree.

A call with no arguments is simply a `call` node with one child.

```cpp
auto nullary = dsl::node<"call">(dsl::leaf<"ident">("f"));
// (call (ident f))
```

Either form is built with the same syntax, differing only in how many children are supplied.

## 11.17 Building a Conditional Tree

An `if` expression with a condition, a then-branch, and an else-branch is a three-child node. Mixed children, some leaves and some inner nodes, are perfectly acceptable.

```cpp
auto if_tree = dsl::node<"if">(
    dsl::node<"<">(
        dsl::leaf<"ident">("x"),
        dsl::leaf<"num">(0)),
    dsl::leaf<"ident">("neg"),
    dsl::leaf<"ident">("pos"));
std::cout << if_tree.dump() << "\n";
// (if (< (ident x) (num 0)) (ident neg) (ident pos))
```

The condition is itself an inner node comparing `x` to zero. The two branches are identifier leaves. A tree like this could be the input to a simplifier that, for example, propagates a known sign of `x` and rewrites the conditional to one of its branches (Chapter 13).

Mixed construction is the rule rather than the exception. Real ASTs almost always combine leaves for atoms and inner nodes for compound forms, and the factories place no restriction on the mixture.

## 11.18 A Larger Program Example

Putting the pieces together, a small straight-line program can be assembled as a single tree. The example below builds a `block` containing an assignment and a return.

```cpp
auto program = dsl::node<"block">(
    dsl::node<":=">(
        dsl::leaf<"ident">("x"),
        dsl::node<"+">(
            dsl::leaf<"ident">("x"),
            dsl::leaf<"num">(1))),
    dsl::node<"return">(
        dsl::leaf<"ident">("x")));
std::cout << program.dump() << "\n";
// (block (:= (ident x) (+ (ident x) (num 1))) (return (ident x)))
```

The `block` node has two children, processed in order. A traversal (Chapter 12) walking this tree would visit the assignment first and the return second, which is the intended program order.

This example also illustrates how the factories scale. There is no special syntax for "a program"; a program is simply a `block` node whose children are statements, which are themselves nodes. The same two factories suffice for trees of any size.

## 11.19 Value Semantics Reaffirmed

Every tree built in this chapter is a value. The factories return `ASTNode` prvalues, and binding them to an `auto` variable copies or moves the node as appropriate. There is no hidden allocation beyond what `std::vector` and `std::string` already perform, and no reference counting.

Because nodes are values, a tree can be copied freely. Copying a node copies its tag, its value, and recursively all of its children. Two copies of a tree are independent: modifying one (by mutating its `children()` vector, for example) does not affect the other.

```cpp
auto a = dsl::node<"+">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
auto b = a;                       // deep copy
b.children().push_back(dsl::leaf<"num">(3));
// a still has two children; b now has three
```

This is the same value-semantics contract described in Chapter 10. The factories do not alter it; they merely produce the values that participate in it.

## 11.20 Why Factories over Direct Construction

It is worth pausing to compare the factory form with the equivalent direct construction. The same two-child addition built directly is:

```cpp
dsl::ASTNode add{"+",
    std::vector<dsl::ASTNode>{
        dsl::ASTNode{"num", "1"},
        dsl::ASTNode{"num", "2"}}};
```

This is correct but clumsy. The tag is written as a string literal at run time, the children vector is spelled out explicitly, and each child must be constructed with its tag and value as positional arguments. The intent of the code is obscured by the machinery.

The factory form restores the intent.

```cpp
auto add = dsl::node<"+">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
```

The tag is in the template argument list, where it visually belongs. The children are simply listed. The leaf values are accepted in their natural types. Brevity and clarity are the only reasons to prefer the factories, but they are sufficient reasons in practice.

## 11.21 Pitfall: Run-Time Tags

The most common mistake with the factories is attempting to pass a run-time string as the tag. Because the tag is a non-type template parameter, this fails to compile.

```cpp
std::string tag = "add";
auto t = dsl::node<tag>(...);   // error: 'tag' is not a constant expression
```

The fix is either to use a literal in the template argument list or, if the tag genuinely varies at run time, to construct the `ASTNode` directly.

```cpp
dsl::ASTNode t{tag, std::vector<dsl::ASTNode>{...}};
```

The direct constructor is the escape hatch for cases the factories do not cover. It is not deprecated or discouraged; it is simply not the primary interface.

## 11.22 Pitfall: Wrong Argument Types for Children

A second common mistake is passing a non-`ASTNode` argument to `node`. The `requires` clause rejects this at compile time, but the diagnostic can be cryptic.

```cpp
auto t = dsl::node<"+">(1, 2);   // error: int is not convertible to ASTNode
```

The factory does not infer a tag for bare values. Each value must be wrapped in a `leaf` call that names its tag explicitly.

```cpp
auto t = dsl::node<"+">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
```

This is a deliberate design choice. Tags are part of the meaning of a node, and the library refuses to guess them.

## 11.23 Pitfall: Ordering of Children

A third pitfall is assuming the factories sort or deduplicate children. They do not. Children are stored in the order given, duplicates and all.

```cpp
auto t = dsl::node<"+">(
    dsl::leaf<"num">(1),
    dsl::leaf<"num">(1));
// two distinct children, both (num 1)
```

For an addition this is harmless. For a node where order is semantically meaningful, such as an assignment `(:= target value)`, swapping the children produces a different and probably incorrect tree. The factories preserve order faithfully; correctness is the caller's responsibility.

## 11.24 Combining with Traversal

The trees built by `leaf` and `node` are the input to every later stage of a DSL pipeline. Chapter 12 documents the traversal helpers that walk a tree and the `dump` function that renders it. The S-expression output shown throughout this chapter is produced by `ASTNode::dump`, which the factories' output supports directly.

```cpp
auto t = dsl::node<"+">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
t.dump();   // "(+ (num 1) (num 2))"
```

A traversal that needs to visit every leaf, for example, recurses through `children()` and stops at `is_leaf()`. The factories guarantee the structure that such a traversal expects: leaves have empty `children()`, inner nodes have empty `value()`.

## 11.25 Combining with Rewriting

Chapter 13 documents the rewrite system, which matches patterns against trees and replaces them with transformed trees. The input to a rewrite is always an `ASTNode` built by these factories, and the output is another `ASTNode` built the same way.

The rewrite example from the library's test suite constructs a double-negation tree using the factories directly.

```cpp
auto in = dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"num">(4)));
auto out = rules.apply(in);
std::cout << out.dump() << "\n";   // (num 4)
```

The `neg` nodes and the `num` leaf are all built with the factories documented here. The rewrite system itself is the subject of later chapters; the point for now is that the factories are the universal entry point for tree construction, whether the tree is an input, an intermediate, or an output.

## 11.26 Choosing Tags

The factories place no constraints on the tag string beyond the limits of `FixedString` (Chapter 4). Tags may be operator-like (`"+"`, `":="`, `"<"`) or word-like (`"add"`, `"assign"`, `"lt"`). The choice is a matter of convention for each DSL.

A consistent convention pays off when the tree is inspected later. A rewrite rule that matches on `tag() == "add"` will miss nodes tagged `"+"`, so the convention should be agreed upon before the tree is built and applied uniformly.

Short tags keep `dump` output compact, which is useful during debugging. Word-like tags are more self-documenting and may be preferred in larger trees. Neither choice affects performance; the tag is a `std::string` inside the node regardless.

## 11.27 Building Trees from Parser Output

In practice, trees are rarely written out by hand as in the examples above. More often they are constructed by a parser that has recognized a piece of input and emits the corresponding node. The factories are equally suited to this case.

A parser combinator (Chapter 23) that recognizes an integer literal can emit a `dsl::leaf<"num">(value)` in its semantic action. A combinator that recognizes a binary expression can emit a `dsl::node<op>(left, right)` from the values produced by its sub-parsers. Because the factories are ordinary functions, they compose naturally with the parser combinator framework.

The same principle applies to the PEG matcher of Chapter 28. Whatever the source of the recognized text, the resulting tree is assembled with the same two factories.

## 11.28 A Note on Performance

The factories are thin wrappers around the `ASTNode` constructors. A `leaf` call performs one stream insertion (or one string copy, for the string overloads) and one `ASTNode` construction. A `node` call performs one vector allocation, a sequence of `emplace_back` calls, and one `ASTNode` construction.

The vector allocation is the most significant cost. The `reserve(sizeof...(children))` call ensures a single allocation sized exactly to the number of children, so there is no reallocation as the pack is expanded. For very large trees the dominant cost remains the recursive construction of children, which is inherent in the tree's size.

There is no allocator customization in the factories. Nodes use the default allocator for `std::vector` and `std::string`. Code with specialized allocation needs should construct `ASTNode` objects directly.

## 11.29 Interoperability with Direct Construction

The factories and the direct constructors produce identical `ASTNode` objects. A tree can mix the two freely, with some nodes built by factories and others constructed directly.

```cpp
dsl::ASTNode direct{"num", "0"};
auto wrapped = dsl::node<"+">(dsl::leaf<"num">(1), direct);
// (+ (num 1) (num 0))
```

Here the first child is built by `leaf` and the second by the direct constructor. The `+` node built by `node` accepts both, because both are `ASTNode` values. The `requires` clause is satisfied by the direct constructor's result just as it is by a factory's result.

This interoperability means the factories can be adopted incrementally. Existing code that constructs nodes directly need not be rewritten; new code can use the factories, and the two will combine without friction.

## 11.30 Summary

`dsl::leaf<Tag>(value)` and `dsl::node<Tag>(children...)` are the primary constructors for `ASTNode` trees. They lift the tag into the template argument list as a `FixedString` non-type template parameter (Chapter 4) and accept the run-time payload as ordinary function arguments.

`leaf` produces a leaf node: a tag plus a textual value, with no children. Its general overload streams the value through an `ostringstream`, so any type with a stream insertion operator is accepted. Two overloads handle `std::string_view` and `const char *` directly, skipping the stream.

`node` produces an inner node: a tag plus a vector of children, with no value. It is variadic, accepting any number of children subject to the constraint that each type be convertible to `ASTNode`. Children are stored in the order given, via a fold expression over `emplace_back`.

Both factories return `ASTNode` prvalues, so trees compose by nesting and move into their parents without copying. The `AST` feature tag exposes the same factories as `make_leaf` and `make_node` member functions for DSLs that derive from `dsl::DSL`.

The factories are ergonomic wrappers, not a separate type system. Their output is indistinguishable from direct construction and interoperates with it freely. They are the idiomatic way to build the trees that traversal (Chapter 12), rewriting (Chapter 13), and the later expression-template facilities (Chapter 15) consume.
