# Chapter 10: The `ASTNode` Value Type

## Overview

DSLtk represents parsed syntax trees with a single, self-contained value type: `dsl::ASTNode`. An `ASTNode` is a recursive class that owns its tag, its text payload, and a vector of child nodes. Every node is either a leaf carrying a string value or an inner node carrying zero or more subtrees. This chapter documents the structure, constructors, accessors, value semantics, and the surrounding `AST` feature tag that brings the AST vocabulary into a user's DSL.

The `ASTNode` type is the common currency of every tree-shaped operation in DSLtk. Chapter 11 builds trees from `dsl::leaf<>` and `dsl::node<>`, which are thin factories over `ASTNode`. Chapter 12 traverses and dumps trees whose roots are `ASTNode` instances. Chapter 13 rewrites them in place. Understanding the value type itself is therefore a prerequisite for the chapters that follow.

This chapter is concerned only with the type itself: its invariants, its constructors, its accessors, and the rules that govern copying, moving, and lifetime. It uses small, self-contained examples constructed by hand. The higher-level builders are introduced in Chapter 11; here we construct `ASTNode` values directly so that the type's contract is unmistakable.

## The Role of `ASTNode`

An abstract syntax tree, in DSLtk's model, is a tree of homogeneous nodes. There is no per-grammar node class hierarchy: a numeric literal, a binary operator, a function call, and an `if` statement are all represented by the same class. What distinguishes one node from another is the contents of its `tag_` string and the shape of its `children_` vector. This homogeneous design keeps the type system simple and lets generic algorithms in later chapters operate uniformly over any tree.

A leaf node carries a string value and no children. An inner node carries children and an empty value. The distinction is recorded explicitly in the `is_leaf_` flag rather than inferred from the emptiness of the children vector, so an inner node with zero children is well-defined and distinct from a leaf. This matters when modelling forms such as empty argument lists or empty blocks, where the node kind matters even though the child list is empty.

The tag is stored as a `std::string` internally, but in typical usage it originates from a `FixedString` non-type template parameter (Chapter 4). The free functions `dsl::leaf<Tag>` and `dsl::node<Tag>` extract a `std::string_view` from the `FixedString` and forward it to the `ASTNode` constructor. The tag is therefore fixed at the call site at compile time, while the node itself stores a run-time copy. This split lets `ASTNode` remain a single non-template type that can be stored in homogeneous containers.

The text payload, `value_`, is a `std::string` holding the lexed or computed spelling of a leaf. For a numeric literal it might be `"42"`; for an identifier, `"foo"`. Because it is a string, the type deliberately does not commit to a single value representation; callers convert to integers, floats, or symbols as needed at the point of use. Inner nodes leave `value_` empty.

The children vector, `children_`, is a `std::vector<ASTNode>`. Because the element type is the node type itself, the structure is recursively defined and entirely owning. A node owns its children, which own their children, and so on down to the leaves. Destroying a root node destroys the entire tree; there are no smart pointers to manage and no shared ownership to reason about.

## The Class Definition

The full declaration of `ASTNode` is small enough to read in one pass. It lives in the `dsl` namespace inside `DSLtk.hpp`. The public interface consists of constructors, inspectors, mutators, equality, and a dump helper; the private section holds the four data members.

```cpp
namespace dsl {

class ASTNode
{
public:
  ASTNode () : tag_ (""), is_leaf_ (true) {}

  /// Construct a leaf node.
  ASTNode (std::string_view tag, std::string value)
      : tag_ (tag), value_ (std::move (value)), is_leaf_ (true)
  {
  }

  /// Construct an inner node with children.
  ASTNode (std::string_view tag, std::vector<ASTNode> children)
      : tag_ (tag), children_ (std::move (children)), is_leaf_ (false)
  {
  }

  std::string_view tag () const { return tag_; }
  bool is_leaf () const { return is_leaf_; }
  const std::string & value () const { return value_; }
  const std::vector<ASTNode> & children () const { return children_; }
  std::vector<ASTNode> & children () { return children_; }
  const ASTNode & child (std::size_t i) const { return children_.at (i); }
  ASTNode & child (std::size_t i) { return children_.at (i); }
  std::size_t size () const { return children_.size (); }

  std::string dump () const;
  bool operator== (const ASTNode &other) const;

private:
  std::string tag_;
  std::string value_;
  std::vector<ASTNode> children_;
  bool is_leaf_ = true;
};

} // namespace dsl
```

The class is a regular value type. It is implicitly copyable and movable because all of its members are copyable and movable; no special member functions are declared or deleted. The compiler-generated copy constructor performs a member-wise deep copy, and the compiler-generated move constructor steals the vector's storage. This is the property that makes `ASTNode` safe to return by value, store in vectors, and pass through value-taking APIs without manual resource management.

## The Four Data Members

The private state of an `ASTNode` is the union of four members. Each plays a specific role, and together they encode everything the tree algorithms need.

The first member is `tag_`, a `std::string`. Although the tag is most often supplied through a `FixedString` template parameter on `dsl::leaf<>` or `dsl::node<>`, the node itself stores an owned `std::string` copy. This means a node carries its tag with it independently of the template context that created it, and tags may also be constructed directly from arbitrary `std::string_view` values when a program builds nodes without the template factories.

The second member is `value_`, a `std::string` holding the leaf payload. It is empty for inner nodes. There is no separate variant or `payload` member: the value string is the sole channel for run-time leaf data. This keeps the type small and the layout simple. Callers that need richer leaf data are expected to encode it as text and parse it back out, or to maintain a side table keyed by node identity.

The third member is `children_`, a `std::vector<ASTNode>`. For leaves it is empty; for inner nodes it holds the owned subtrees in source order. The vector is the sole owner of its elements, and because each element is itself an owning `ASTNode`, the entire subtree is owned transitively by the root. There is no sharing, no aliasing, and no cycle support: trees are genuine trees, not graphs.

The fourth member is `is_leaf_`, a `bool` defaulting to `true`. It records the node's kind explicitly so that an inner node with zero children is distinguishable from a leaf with an empty value. The leaf constructor sets it to `true`; the inner constructor sets it to `false`; the default constructor leaves it at `true`. It is consulted by `is_leaf()`, `dump()`, and `operator==`, and it is the single bit that prevents ambiguity in the empty-children case.

## Constructors

`ASTNode` provides three constructors, plus the implicitly generated copy and move operations. Each constructor establishes a specific invariant over the four data members.

The default constructor takes no arguments and produces a leaf node with an empty tag, an empty value, and no children. It exists so that `ASTNode` can be default-constructed, which is required for use in some standard containers and as a value to be assigned into later. A default-constructed node is rarely useful as a final tree element, but it is a valid placeholder.

```cpp
dsl::ASTNode empty;          // default-constructed leaf
assert(empty.is_leaf());
assert(empty.tag().empty());
assert(empty.value().empty());
assert(empty.size() == 0);
```

The leaf constructor takes a `std::string_view tag` and a `std::string value`. It moves the value into `value_`, copies the tag into `tag_`, sets `is_leaf_` to `true`, and leaves `children_` empty. The two arguments are the only data a leaf carries. This constructor is the one called by `dsl::leaf<>` after it has rendered its argument to a string.

The inner constructor takes a `std::string_view tag` and a `std::vector<ASTNode> children`. It moves the vector into `children_`, copies the tag, sets `is_leaf_` to `false`, and leaves `value_` empty. The vector is moved in as a whole, which is efficient when the caller has already assembled the children. The `dsl::node<>` factory uses `emplace_back` to build the vector and then moves it into this constructor.

Because copy and move are compiler-generated, returning an `ASTNode` from a function is cheap when the move elides or steals the vector. A moved-from `ASTNode` is left in a valid but unspecified state; in practice its vector is empty and its strings may be empty, but the only safe operation on a moved-from node is destruction or reassignment.

## Constructing Nodes by Hand

Although Chapter 11 introduces the `dsl::leaf<>` and `dsl::node<>` factories, it is instructive to build `ASTNode` values directly. Doing so makes the type's contract visible: a leaf is a tag plus a string, and an inner node is a tag plus a vector.

```cpp
#include "DSLtk.hpp"
#include <vector>

int main() {
  // A numeric literal leaf, built directly.
  dsl::ASTNode num{"num", std::string{"42"}};

  // An identifier leaf.
  dsl::ASTNode id{"ident", std::string{"foo"}};

  // An inner node with two children, built from a brace-initialised vector.
  std::vector<dsl::ASTNode> kids;
  kids.emplace_back("num", std::string{"1"});
  kids.emplace_back("num", std::string{"2"});
  dsl::ASTNode add{"add", std::move(kids)};

  return 0;
}
```

The leaf form is the simplest: a tag string view and a value string. The inner form requires a `std::vector<ASTNode>`, which the caller is free to assemble with `emplace_back`, `push_back`, or initializer lists. Because the inner constructor takes the vector by value and moves it, passing a named vector with `std::move` avoids a copy.

A brace-enclosed initializer list can also be used to construct the children vector directly, which yields a concise literal syntax for small trees:

```cpp
dsl::ASTNode add{"add",
  std::vector<dsl::ASTNode>{
    {"num", std::string{"1"}},
    {"num", std::string{"2"}}
  }};
```

This form is verbose because each leaf must spell `std::string{}` for its value argument; the `dsl::leaf<>` factory exists precisely to remove that verbosity. Nonetheless, the direct construction works and demonstrates that `ASTNode` has no hidden friends or privileged factories. Everything the factories do, a caller can do by hand.

## Leaf versus Inner Nodes

The `is_leaf()` accessor returns the `is_leaf_` flag. It is the canonical test for node kind and should be preferred over testing `children().empty()`, because an inner node may legitimately have zero children. The dump implementation and the equality operator both consult `is_leaf_` rather than the children count, and user code should do the same.

A leaf node answers `is_leaf()` with `true`, has an empty `children()` vector, and may carry a non-empty `value()`. An inner node answers `is_leaf()` with `false`, has a `children()` vector of any length including zero, and has an empty `value()`. These two cases are exhaustive: every `ASTNode` is one or the other, and the constructors are the only way to set the flag.

The distinction matters when modelling grammatical forms that have structure but no content of their own. An empty statement block, for example, is naturally an inner node with tag `"block"` and no children; it is not a leaf, and treating it as one would lose the structural information that the form is a block. Conversely, a leaf with an empty value is a legitimate leaf, not a degenerate inner node.

```cpp
dsl::ASTNode empty_block{"block", std::vector<dsl::ASTNode>{}};
assert(!empty_block.is_leaf());
assert(empty_block.size() == 0);

dsl::ASTNode empty_leaf{"nil", std::string{}};
assert(empty_leaf.is_leaf());
assert(empty_leaf.value().empty());
```

## The `tag()` Accessor

The `tag()` member returns a `std::string_view` over the stored tag. The view is valid for as long as the `ASTNode` itself lives and is not mutated. Because the tag is stored as an owned `std::string`, the view dangles once the node is destroyed or moved-from; callers that need a tag with independent lifetime should construct a `std::string` from the view.

Tags are compared for equality as part of `operator==`, and they are the primary key by which traversal and rewrite algorithms identify node kinds. By convention, tags are short, lowercase identifiers such as `"num"`, `"ident"`, `"add"`, `"mul"`, `"call"`, `"if"`, and `"block"`. DSLtk does not enforce a vocabulary; the tag is an ordinary string, and the set of tags is whatever the program chooses to use.

Because the tag originates from a `FixedString` template parameter in the common case, the same tag string is repeated at every node of the same kind. There is no interning: each `ASTNode` owns its own copy. For very large trees this is a measurable memory cost, but it buys value semantics and avoids any need for a tag table.

## The `value()` Accessor

The `value()` member returns a `const std::string&` to the leaf payload. For inner nodes the string is empty; for leaves it holds the spelled-out value. The reference is valid for the lifetime of the node and is not invalidated by const operations on the node.

The payload is always text. When a leaf represents a number, `value()` returns the digits as a string, and the caller is responsible for parsing. When a leaf represents an identifier, `value()` returns the identifier's name. When a leaf represents a string literal, `value()` returns the literal's text, including or excluding surrounding quotes according to the convention adopted by the builder.

This design keeps `ASTNode` independent of any value domain. The same node type can represent a C expression, a JSON document, or a configuration file; the only thing that changes is the interpretation of the value strings. The cost is that converting between the string payload and a typed value happens at every use site, but for the kind of small, throwaway trees that DSLtk targets, this cost is negligible.

## The `children()` Accessors

Two overloads of `children()` are provided. The const overload returns a `const std::vector<ASTNode>&` and is the one used by inspectors, traversals, and equality. The non-const overload returns a mutable reference, allowing a caller to append, remove, or reorder children in place. Mutation through this reference is the supported way to edit a tree without rebuilding it; the rewrite machinery in Chapter 13 uses it.

The returned reference is valid for the lifetime of the node. Operations that reallocate the vector — such as `push_back` past capacity — invalidate references, pointers, and iterators into the vector, but they do not affect the node's other members or its identity.

Because the children are stored in a `std::vector`, indexed access is O(1) and iteration is cache-friendly. The order of children is the order in which they were inserted, which in practice is source order. DSLtk does not reorder children; any reordering is the caller's responsibility.

## The `child()` Accessors

The `child(i)` member provides checked indexed access. It is provided in both const and mutable overloads and is implemented in terms of `std::vector::at`, which throws `std::out_of_range` if the index is past the end. This is the safest accessor for code that is not certain of the child count.

```cpp
const dsl::ASTNode &c = add.child(0);
std::cout << c.tag() << ' ' << c.value() << '\n';
```

For code that has already checked `size()` or that is willing to accept undefined behaviour on a logic error, direct access through `children()[i]` is faster because it is unchecked. The two styles are interchangeable for correct indices; the choice is a matter of defensive programming taste.

The mutable `child(i)` overload returns a reference through which the child can be replaced, mutated, or further indexed. Assigning a new `ASTNode` to `child(i)` replaces the subtree at that position and destroys the old one. This is the granularity at which local rewrites operate.

## The `size()` Accessor

The `size()` member returns the number of children as a `std::size_t`. For a leaf it is always zero. For an inner node it is the length of the `children_` vector. `size()` is the accessor used by code that needs to know how many children to expect without fetching the whole vector.

Note that `size()` reflects the children count, not the total number of nodes in the subtree. A tree of one root with two leaf children has `size() == 2`, not three. Counting all nodes requires a recursive walk, as shown in the traversal examples later in this chapter.

## Equality

Two `ASTNode` values are equal when their tags are equal, their kinds match (both leaves or both inner), and their payloads match. For leaves, the payloads compared are the `value_` strings. For inner nodes, the payloads compared are the `children_` vectors, which in turn compare element-wise using `ASTNode::operator==`. Equality is therefore structural and recursive.

```cpp
bool
operator== (const ASTNode &other) const
{
  if (tag_ != other.tag_ || is_leaf_ != other.is_leaf_)
    return false;
  if (is_leaf_)
    return value_ == other.value_;
  return children_ == other.children_;
}
```

Structural equality means that two trees with identical shape and spelling compare equal regardless of how they were built. A tree constructed by hand with `ASTNode` constructors equals a tree constructed by `dsl::node<>` and `dsl::leaf<>` as long as the tags and values coincide. This property is essential for testing: expected trees can be written compactly with the factories and compared against trees produced by a parser.

Equality is deep. Comparing two large trees walks both of them fully and returns false as soon as a mismatch is found. There is no hashing and no short-circuit on node identity; every comparison is a content comparison. For repeated comparisons of the same large trees, memoisation (Chapter 18) or caching at a higher level may be appropriate.

The default-constructed node — empty tag, empty value, leaf kind — is equal to any other default-constructed node and unequal to anything else. This gives a natural "null" node value, though DSLtk does not prescribe its use.

## The `dump()` Method

The `dump()` member returns a `std::string` containing an S-expression representation of the tree. Leaves render as `(tag value)` and inner nodes render as `(tag child1 child2 ...)`. The rendering is recursive and produces a single-line string with no indentation; it is intended for diagnostics, logging, and testing, not for human-facing pretty-printing (Chapter 12 covers richer traversal and formatting).

```cpp
std::string
dump () const
{
  std::ostringstream os;
  dump_impl (os);
  return os.str ();
}
```

The private `dump_impl` helper writes to an `std::ostringstream`. For a leaf it writes the tag followed by the value if the value is non-empty. For an inner node it writes the tag followed by each child's rendering separated by spaces. Every node is wrapped in parentheses, so the output is unambiguous and parseable by a simple S-expression reader.

The dump format is stable: it depends only on the node's tag, kind, value, and children, and not on the order of insertion beyond the children order. Two equal nodes always dump to the same string. This makes `dump()` useful as a canonical form in tests:

```cpp
auto expr = dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
assert(expr.dump() == "(add (num 1) (num 2))");
```

## Value Semantics

`ASTNode` is a value type. Copying an `ASTNode` copies its tag string, its value string, its children vector, and its flag. Because the vector's elements are themselves `ASTNode` values, the copy is deep: the entire subtree is duplicated. There is no copy-on-write, no sharing, and no implicit reference semantics.

Moving an `ASTNode` moves each member. The tag and value strings are moved (which typically steals any heap allocation), and the children vector is moved (which steals its buffer in constant time). After a move, the source node is in a valid but unspecified state; its members are valid objects but their contents are not guaranteed. The move is cheap relative to a copy, and returning nodes by value is the idiom of choice.

Because copies are deep, large trees are expensive to copy. A tree with ten thousand nodes copied through a value-taking function will allocate ten thousand strings and vectors. Code that handles large trees should pass `ASTNode` by const reference unless it needs to own or mutate a copy. The traversal and rewrite algorithms in Chapters 12 and 13 take const references and mutate in place through the non-const `children()` accessor where needed.

The absence of shared ownership is deliberate. Shared ownership would complicate the type, introduce aliasing, and make mutation surprising. By making each node the sole owner of its children, DSLtk ensures that a node's subtree is never modified behind its back. The cost is the copy cost, which is acceptable for the trees the toolkit is designed for.

## Lifetime and Returning Nodes

Returning an `ASTNode` from a function by value is the supported way to build a tree. Thanks to mandatory copy elision in C++20 and the compiler-generated move constructor, a returned node is constructed in place at the call site in the common case, with no copy and no move. When a move does occur, it steals the vector's buffer rather than copying the children.

```cpp
dsl::ASTNode make_expr() {
  std::vector<dsl::ASTNode> kids;
  kids.emplace_back("num", std::string{"1"});
  kids.emplace_back("num", std::string{"2"});
  return dsl::ASTNode{"add", std::move(kids)};
}
```

A node returned by value can be stored in a local variable, moved into a parent's children vector, or passed to another function. References obtained from a returned node — for example, `const ASTNode&` — are valid for as long as the node is kept alive, which is typically the lifetime of the local or member that holds the returned value. Returning a reference to a node's child from a function that returns the node by value is a dangling-reference hazard, because the temporary node is destroyed at the end of the full expression.

When a node is stored as a member of another object, its lifetime is bound to that object. Storing `ASTNode` in a `std::vector`, a `std::map`, or a class member is safe and requires no special care beyond the usual rules for the container's exception safety. Reallocating a `std::vector<ASTNode>` moves the nodes if the move constructor is `noexcept`, which it is for `std::string` and `std::vector` members on conforming implementations.

## Building a Literal Number Node

A literal number is the simplest leaf. It has a tag identifying it as a number and a value string holding the digits. Constructing one by hand is a single call to the leaf constructor.

```cpp
dsl::ASTNode forty_two{"num", std::string{"42"}};
assert(forty_two.is_leaf());
assert(forty_two.tag() == "num");
assert(forty_two.value() == "42");
assert(forty_two.size() == 0);
```

The value is the text `"42"`, not the integer `42`. Conversion to an integer is the caller's responsibility; `std::stoi` or `std::from_chars` are the natural choices. By keeping the value as a string, `ASTNode` avoids committing to a numeric type and can represent numbers of any size or precision without modification.

A number leaf with a negative value, a fractional value, or a hexadecimal value is represented identically: the tag is `"num"` and the value is the appropriate text. The grammar or builder that produced the leaf decides how the text is spelled; the node itself is agnostic.

## Building a Binary-Op Node

A binary operation such as `(+ a b)` is naturally an inner node with three children: the operator and the two operands. In DSLtk's convention the operator is often folded into the tag, yielding a node with tag `"add"` and two operand children.

```cpp
dsl::ASTNode add_expr{"add",
  std::vector<dsl::ASTNode>{
    {"ident", std::string{"a"}},
    {"ident", std::string{"b"}}
  }};
assert(!add_expr.is_leaf());
assert(add_expr.size() == 2);
assert(add_expr.child(0).tag() == "ident");
assert(add_expr.child(0).value() == "a");
```

The children are in source order: the left operand first, the right operand second. The order is significant for non-commutative operations and must be preserved by any transformation. The `child(0)` and `child(1)` accessors retrieve the operands with bounds checking; direct indexing through `children()` is equivalent but unchecked.

A binary-op node may itself be the child of another binary-op node, building up an expression tree of arbitrary depth. Because each node owns its children, the root of such a tree owns the entire structure, and destroying the root reclaims everything.

## Building a Function-Call Node

A function call has a callee and a list of arguments. A common representation is an inner node with tag `"call"`, a first child naming the function, and subsequent children for each argument.

```cpp
dsl::ASTNode call_expr{"call",
  std::vector<dsl::ASTNode>{
    {"ident", std::string{"f"}},
    {"num", std::string{"1"}},
    {"num", std::string{"2"}}
  }};
assert(call_expr.size() == 3);
assert(call_expr.dump() == "(call (ident f) (num 1) (num 2))");
```

The argument list is simply the tail of the children vector. There is no separate argument-list node unless the grammar introduces one. Empty argument lists are represented by an inner node with only the callee child, or by an inner node with zero children if the callee is encoded in the tag — a design choice left to the program.

Nesting calls is a matter of nesting `ASTNode` values. The inner call is constructed first and moved into the outer call's children vector. Because moves are cheap, deep nesting is not a performance concern at construction time.

## Building an If-Statement Node

An `if` statement has a condition, a then-branch, and an else-branch. It is naturally an inner node with three children. The tag distinguishes it from other statement forms.

```cpp
dsl::ASTNode if_stmt{"if",
  std::vector<dsl::ASTNode>{
    {"call", std::vector<dsl::ASTNode>{
      {"ident", std::string{"<"}},
      {"ident", std::string{"x"}},
      {"num", std::string{"10"}}
    }},
    {"block", std::vector<dsl::ASTNode>{
      {"call", std::vector<dsl::ASTNode>{
        {"ident", std::string{"print"}},
        {"str", std::string{"low"}}
      }}
    }},
    {"block", std::vector<dsl::ASTNode>{}}
  }};
assert(if_stmt.size() == 3);
assert(if_stmt.child(0).tag() == "call");
assert(if_stmt.child(2).is_leaf() == false);
assert(if_stmt.child(2).size() == 0);
```

The third child — the else block — is an inner node with zero children, modelling an empty block. This is the case where the explicit `is_leaf_` flag matters: the node is not a leaf, even though it has no children, because its tag says it is a block. Treating it as a leaf would lose the structural information.

The dump of this node is a single-line S-expression that captures the entire structure:

```cpp
std::cout << if_stmt.dump() << '\n';
// (if (call (ident <) (ident x) (num 10)) (block (call (ident print) (str low))) (block))
```

## Inspecting a Tree

Inspecting a tree is a matter of reading the tag, the kind, and the value, and recursing into the children. A minimal recursive printer reads each node and recurses on its children:

```cpp
#include "DSLtk.hpp"
#include <iostream>

void print(const dsl::ASTNode& n, int depth = 0) {
  for (int i = 0; i < depth; ++i) std::cout << "  ";
  std::cout << n.tag();
  if (n.is_leaf()) {
    if (!n.value().empty()) std::cout << ' ' << n.value();
    std::cout << '\n';
  } else {
    std::cout << '\n';
    for (const auto& c : n.children()) print(c, depth + 1);
  }
}
```

The range-for over `children()` is the idiomatic way to visit every child. Because `children()` returns a const reference for a const node, no copy is made. The recursion bottoms out at leaves, which have no children and therefore do not recurse.

Reading the tag at each node lets the inspector dispatch on node kind. A more elaborate inspector might switch on the tag to print different forms differently, or to extract typed values from the value string. The homogeneous nature of `ASTNode` means the dispatch is by string comparison, not by virtual function; for the tree sizes DSLtk targets, this is fast enough.

Counting nodes is a similar recursion. The example from the DSLtk distribution is illustrative:

```cpp
int count_nodes(const dsl::ASTNode& n) {
  int total = 1;
  for (const auto& c : n.children()) total += count_nodes(c);
  return total;
}
```

This counts every node in the subtree, including the root. It works for any tree regardless of shape, because it relies only on the `children()` accessor and recursion. It is the template for most tree-walking algorithms in later chapters.

## The `AST` Feature Tag

The `AST` feature tag is a small struct that brings the AST vocabulary into a DSL built with the CRTP base (Chapter 3). It provides a Mixin with two static member functions, `make_leaf` and `make_node`, which forward to the free functions `dsl::leaf<>` and `dsl::node<>`. The feature tag is the hook through which a DSL author opts into the AST utilities.

```cpp
struct AST
{
  template <typename Derived> struct Mixin
  {
    template <FixedString Tag, typename T>
    static ASTNode make_leaf (T &&val)
    {
      return dsl::leaf<Tag> (std::forward<T> (val));
    }

    template <FixedString Tag, typename... Children>
      requires (std::convertible_to<Children, ASTNode> && ...)
    static ASTNode make_node (Children &&...children)
    {
      return dsl::node<Tag> (std::forward<Children> (children)...);
    }
  };
};
```

The Mixin's methods are thin wrappers. They do not add behaviour beyond what the free functions provide; they simply make the factories reachable as `MyDSL::make_leaf<"num">(42)` instead of `dsl::leaf<"num">(42)`. A DSL that derives from `dsl::DSL<MyDSL, dsl::AST>` gains these names as inherited static members.

The node types themselves are free in the `dsl` namespace. `ASTNode`, `leaf`, and `node` are available whether or not a DSL opts into the `AST` feature. The feature tag is therefore a convenience for code that wants the factories namespaced under its own DSL type, not a prerequisite for using ASTs. Code that works directly with `ASTNode` need not involve the feature tag at all.

## Using the `AST` Mixin

A DSL that opts into the `AST` feature declares it in the feature list of its CRTP base. The Mixin is then available as inherited static members, and the DSL's own name can be used as the qualification prefix for the factories.

```cpp
struct MyDSL : dsl::DSL<MyDSL, dsl::AST> {};

int main() {
  auto tree = MyDSL::make_node<"root">(MyDSL::make_leaf<"val">(42));
  std::cout << tree.dump() << '\n';
  // (root (val 42))
}
```

The two calls are equivalent to `dsl::node<"root">(dsl::leaf<"val">(42))`; the only difference is the qualification. The choice between the two styles is one of namespacing and intent. Using the DSL's own name makes the code's affiliation with a particular DSL explicit, which can be useful in code that mixes multiple DSLs.

The Mixin's `make_leaf` and `make_node` are static members, so they can be called without an instance. They are also available through an instance, as shown in the feature tag's documentation, but no instance state is used. The Mixin adds no data members to the derived DSL.

## Pitfalls

The tag is a `std::string` stored in the node, even though it is most often supplied through a `FixedString` template parameter. This means the tag is fixed at the type level at the call site — the `FixedString` parameter is part of the function template's signature — but the node carries a run-time copy. Two nodes with the same `FixedString` tag have equal `std::string` tags but are not otherwise connected. There is no shared tag identity to compare by pointer.

The text payload is run-time and untyped. Converting it to a number, a flag, or an enumeration is the caller's responsibility and is a common source of errors. A leaf with value `"42"` is not the same as a leaf with value `" 42"` or `"42.0"`; equality is textual, not semantic. Code that compares leaves by parsed value rather than by string must perform the parse explicitly.

Deep copy is the rule, not the exception. Passing an `ASTNode` by value to a function copies the entire subtree, which can be expensive for large trees. The same applies to storing nodes in containers that copy on insertion, such as `std::vector` when it reallocates. Prefer const references for inspection, and reserve container capacity or use move insertion where possible.

Lifetime is straightforward but unforgiving. A reference to a child obtained through `children()` or `child()` is valid only while the parent node is alive and unmutated in a way that reallocates the vector. Returning a reference to a child of a temporary node is a dangling reference. When in doubt, return a copy or restructure the code so that the parent outlives the use of the reference.

The `is_leaf_` flag is set at construction and never re-derived. If a program mutates an inner node by clearing its children vector through the non-const `children()` accessor, the node remains an inner node — `is_leaf()` still returns `false`. The flag is not a function of the children count; it is an independent piece of state. Code that mutates children must keep this in mind, and code that needs a "structurally empty" notion should test `size() == 0` rather than `is_leaf()`.

## Summary

`dsl::ASTNode` is a regular, homogeneous value type that represents a node of a syntax tree. It carries a tag string, a value string, a vector of child nodes, and a leaf flag. Leaves hold their payload in `value_` and have no children; inner nodes hold their subtrees in `children_` and have an empty value. The flag distinguishes the two kinds even when an inner node has zero children.

The constructors are few: a default constructor, a leaf constructor taking tag and value, and an inner constructor taking tag and a children vector. Copy and move are compiler-generated and behave as expected — deep copy, cheap move. Accessors expose the tag, the value, the children vector, indexed children, and the children count. Equality is structural and recursive. `dump()` renders the tree as a single-line S-expression suitable for testing and diagnostics.

The `AST` feature tag is a marker mixin that exposes `make_leaf` and `make_node` as inherited static members of a CRTP-derived DSL. The node types and factories are free in namespace `dsl`, so the feature tag is a convenience, not a prerequisite. Building trees by hand is straightforward using the constructors directly; the factories in Chapter 11 remove the verbosity of spelling `std::string{}` for every leaf value. Traversal, dumping, and rewriting are covered in Chapters 12 and 13 and build directly on the value type documented here.
