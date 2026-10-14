# Chapter 12: Traversing and Dumping ASTs

An abstract syntax tree, once built, is rarely useful unless it can be inspected, measured, transformed, and rendered back into a human-readable form. Chapter 10 introduced the `dsl::ASTNode` value type and Chapter 11 showed how the `dsl::leaf<Tag>` and `dsl::node<Tag>` builders construct trees. This chapter turns to the consumer side: walking those trees, reading data out of them, and producing textual representations for debugging and diagnostics.

Traversal of an `ASTNode` is, by design, an ordinary recursive operation over an ordinary C++ value type. There is no visitor base class, no double-dispatch machinery, and no callback registration. The `children()` member exposes a `const std::vector<ASTNode>&`, and a pre-order walk is therefore a simple range-based `for` loop inside a recursive function. This deliberate plainness is the point: trees are data, and the library gets out of the way of the algorithm.

The same plainness extends to output. Every `ASTNode` provides a `dump()` member that returns a single-line S-expression string. There is no streaming overload and no indentation parameter; the format is fixed and predictable. When richer rendering is required — colored output, indented multi-line boxes, annotated nodes — the application supplies its own walker, using `dump()` only as a fallback for quick inspection.

The material below covers the traversal primitives, the dump format, several families of useful queries (counting, leaf collection, depth, search), mutation during traversal, and the bridge to the rewrite machinery of Chapters 13 and 14. All examples compile against `-std=c++20` with only `DSLtk.hpp` included.

## The ASTNode Inspection Surface

Before walking a tree it is worth restating the exact set of accessors available on `dsl::ASTNode`, because traversal code is built entirely from these members. The class is declared in `DSLtk.hpp` and is a regular value type: copyable, movable, and equality-comparable.

```cpp
class ASTNode {
public:
  std::string_view tag () const;
  bool            is_leaf () const;
  const std::string &          value () const;     // leaf payload
  const std::vector<ASTNode> & children () const;  // immutable view
  std::vector<ASTNode> &       children ();        // mutable access
  const ASTNode & child (std::size_t i) const;
  ASTNode &       child (std::size_t i);
  std::size_t     size () const;                   // child count
  std::string     dump () const;                   // S-expression
  bool operator== (const ASTNode & other) const;
};
```

These ten members are everything traversal needs. There is no parent pointer: navigation is top-down only, which keeps nodes cheap to copy and obviates back-pointer maintenance. A bottom-up walk is therefore expressed by recursing into children first and acting on a node after the recursive calls return.

The distinction between `value()` and `children()` mirrors the leaf/inner dichotomy established at construction. A leaf node carries its payload as a `std::string` obtained by streaming the constructor argument through an `ostringstream`; an inner node carries a `vector` of children and an empty value. The `is_leaf()` predicate is the canonical discriminator, and code that branches on it is immune to the representation detail that an inner node's `value()` is empty by convention.

The two overloads of `children()` and `child(i)` provide both `const` and mutable access. Read-only traversals bind to the `const` overloads; structural transformations bind to the mutable ones. The `child(i)` accessor uses `at()` internally, so an out-of-range index throws `std::out_of_range` rather than exhibiting undefined behavior. This is intentional for a value type intended to be robust under ad-hoc inspection.

The `size()` member returns the number of direct children. For a leaf it is always zero. Combined with `child(i)`, it enables the explicit indexed loop style preferred by algorithms that need to know the position of a child, such as rewrite predicates that distinguish a unary from a binary operator by arity.

## The dump() Member

The single built-in rendering primitive is `dump()`. Its declaration is minimal.

```cpp
std::string ASTNode::dump () const;
```

The member takes no arguments and returns a string. Internally it delegates to a private `dump_impl(std::ostringstream&)` that emits the tree as a single-line S-expression. The format is fixed: a leaf renders as `(tag value)` and an inner node renders as `(tag child1 child2 ...)`, with each child recursively expanded and separated by a single space. No indentation, no trailing newline, no quoting of the value.

The private implementation, quoted verbatim from the header, makes the format precise.

```cpp
void ASTNode::dump_impl (std::ostringstream &os) const {
  os << '(' << tag_;
  if (is_leaf_) {
    if (!value_.empty ())
      os << ' ' << value_;
  } else {
    for (auto &c : children_) {
      os << ' ';
      c.dump_impl (os);
    }
  }
  os << ')';
}
```

A leaf whose value is the empty string renders as just `(tag)`. This occurs when `dsl::leaf<Tag>("")` is constructed explicitly, or when an inner node is misused as a leaf. The discriminator is `is_leaf_`, not the emptiness of the value, so the two cases are unambiguous.

The output of `dump()` is suitable for logging, unit-test assertions, and one-line diagnostic messages. Because the format is canonical and equality on `ASTNode` is structural, comparing `a.dump() == b.dump()` is a valid — if coarse — way to test structural equality, though direct `operator==` is preferred.

A small example exercises the format end to end.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto expr = dsl::node<"add">(
      dsl::leaf<"num">(1),
      dsl::leaf<"num">(2));
  std::cout << expr.dump() << "\n";   // (add (num 1) (num 2))

  auto bare = dsl::leaf<"epsilon">("");
  std::cout << bare.dump() << "\n";   // (epsilon)
}
```

The single-line output is compact but becomes unwieldy on deep trees. Later sections show how to build an indented renderer from the public accessors when readability matters more than compactness.

## Recursive Pre-Order Traversal

The most common traversal is a recursive pre-order visit: act on the node, then recurse into each child. Because `children()` returns a `const std::vector<ASTNode>&`, the body of the recursion is a plain range-based loop.

```cpp
#include "DSLtk.hpp"
#include <iostream>

void print_pre(const dsl::ASTNode& n) {
  if (n.is_leaf())
    std::cout << n.tag() << "=" << n.value() << "\n";
  else
    std::cout << n.tag() << "\n";
  for (const auto& c : n.children())
    print_pre(c);
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::leaf<"num">(3),
      dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  print_pre(t);
}
```

This produces a flat listing in parent-before-child order: `mul`, `num=3`, `add`, `num=1`, `num=2`. The order of evaluation follows the order of the `children()` vector, which in turn follows the order in which children were passed to `dsl::node<Tag>(...)`. The library preserves construction order; it never reorders children.

Pre-order traversal is the natural choice when the action on a parent does not depend on properties of its descendants. Printing tags, emitting a linear instruction sequence, and counting nodes all fit this description. When an action does depend on subtree results — computing the value of an expression, type-checking, constant folding — post-order is more appropriate.

The recursive formulation is clear but shares the stack-depth characteristics of any recursive algorithm over a recursive data structure. A degenerate left-heavy or right-heavy tree of depth `N` consumes `N` stack frames. For trees produced by typical parsers this is rarely a concern, but it is worth noting for grammars that admit unbounded repetition translated into chained binary nodes. The Pitfalls section returns to this.

## Recursive Post-Order Traversal

Post-order traversal reverses the action: recurse into the children first, then act on the node. This is the order used by evaluators, because the value of an inner node typically depends on the values of its children.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <vector>

void collect_post(const dsl::ASTNode& n, std::vector<std::string>& out) {
  for (const auto& c : n.children())
    collect_post(c, out);
  out.push_back(std::string(n.tag()));
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::leaf<"num">(3),
      dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  std::vector<std::string> order;
  collect_post(t, order);
  for (const auto& s : order) std::cout << s << " ";
  std::cout << "\n";   // num num add num mul
}
```

The output sequence lists leaves first, then inner nodes from the bottom up, with the root last. This is exactly the order in which a stack-based evaluator would consume the tree to compute a value: push each leaf's value, and when an inner node is reached, pop its operands, apply the operator, and push the result.

Post-order is also the natural order for destruction-like passes, such as releasing resources associated with subtrees before releasing the parent. The discipline of "children before parent" is enforced by the position of the recursive call relative to the action, not by any library mechanism.

A common pattern combines pre- and post-order actions in a single walk: open a scope on the way down, close it on the way up. This is expressed by placing statements both before and after the child loop, which is perfectly legal and frequently useful for emitting bracketed output or managing an indentation stack.

## Explicit Child-Indexed Loops

Range-based loops are convenient when the position of a child is irrelevant. Some algorithms need the index: a binary operator that must distinguish its left from its right operand, a rewrite rule that fires only on the second child, or a formatter that inserts separators between children.

The indexed form uses `size()` and `child(i)`.

```cpp
#include "DSLtk.hpp"
#include <iostream>

void print_indexed(const dsl::ASTNode& n) {
  std::cout << "(" << n.tag();
  for (std::size_t i = 0; i < n.size(); ++i) {
    std::cout << " [#" << i << "] ";
    print_indexed(n.child(i));
  }
  std::cout << ")\n";
}

int main() {
  auto t = dsl::node<"sub">(
      dsl::leaf<"var">("a"),
      dsl::leaf<"var">("b"));
  print_indexed(t);   // (sub [#0] (var a) [#1] (var b) )
}
```

The indexed style is also the right tool when the loop body must skip a child conditionally without disturbing the iteration. Because `child(i)` is a random-access accessor, an algorithm can visit children out of order — for example, visiting the right operand of a commutative operator first when that is more convenient for a transformation.

Both `child(i)` overloads use `at()`, so a buggy loop that runs past `size()` raises `std::out_of_range` rather than reading garbage. This is a deliberate safety trade against raw `operator[]`; the cost is a single bounds check per access, which is negligible next to the recursion itself.

## Reading Node Data

The data carried by a node is small: a tag, a leaf value, and a child sequence. Reading them is direct. The tag is a `std::string_view` owned by the node, valid for the node's lifetime. The value is a `const std::string&`, likewise owned. The children are a `const std::vector<ASTNode>&` that may be empty.

The tag, returned by `tag()`, is the compile-time `FixedString` passed to `dsl::leaf<Tag>` or `dsl::node<Tag>`, narrowed to a runtime `string_view`. Traversal code that switches on the tag typically compares against string literals: `if (n.tag() == "add")`. Because `tag()` returns a `string_view`, such comparisons are efficient and do not allocate.

The value, returned by `value()`, is meaningful only for leaves. For an inner node it is the empty string by construction, but relying on that fact is fragile: `is_leaf()` is the canonical discriminator. Code that reads `value()` should either be guarded by an `is_leaf()` check or be in a context — such as a leaf-collecting visitor — where leaves are the only nodes visited.

The children, returned by `children()`, are a vector of `ASTNode` by value. Each child is a full node, recursively traversable. The const overload returns a reference that is stable for the lifetime of the parent, while the non-const overload returns a reference that is stable until the vector is mutated. These lifetime properties matter when a traversal caches pointers into the tree, as discussed under Pitfalls.

## Counting Nodes

A frequent query is the total number of nodes in a tree. The implementation is a one-liner recursion, and the example distributed with the library — `examples/09-ast-traversal.cpp` — is precisely this program, quoted here verbatim.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int count_nodes(const dsl::ASTNode& n) {
  int total = 1;
  for (const auto& c : n.children()) total += count_nodes(c);
  return total;
}

int main() {
  auto t = dsl::node<"mul">(dsl::leaf<"num">(3), dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  std::cout << count_nodes(t) << "\n";
}
```

The function counts the node itself (`1`) plus the recursive sum over its children. Leaves contribute one each and terminate the recursion because their `children()` is empty. For the tree `(mul (num 3) (add (num 1) (num 2)))` the result is five.

Variations are immediate. Counting only leaves tests `is_leaf()` and skips the loop. Counting inner nodes inverts the test. Counting nodes matching a predicate parameterizes the increment.

```cpp
std::size_t count_leaves(const dsl::ASTNode& n) {
  if (n.is_leaf()) return 1;
  std::size_t total = 0;
  for (const auto& c : n.children()) total += count_leaves(c);
  return total;
}

std::size_t count_tagged(const dsl::ASTNode& n, std::string_view want) {
  std::size_t total = (n.tag() == want) ? 1 : 0;
  for (const auto& c : n.children()) total += count_tagged(c, want);
  return total;
}
```

These queries are pure functions of the tree and are safe to call on `const` references. They allocate no heap memory and run in time linear in the number of nodes.

## Collecting Leaves

Collecting all leaf values into a container is the foundation of identifier listing, literal extraction, and token-level analysis. The pattern appends to a caller-supplied container during a pre-order walk.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <vector>
#include <string>

void gather_leaves(const dsl::ASTNode& n, std::vector<std::string>& out) {
  if (n.is_leaf()) {
    out.emplace_back(n.value());
    return;
  }
  for (const auto& c : n.children())
    gather_leaves(c, out);
}

int main() {
  auto expr = dsl::node<"add">(
      dsl::leaf<"var">("x"),
      dsl::node<"mul">(
          dsl::leaf<"var">("y"),
          dsl::leaf<"num">(2)));
  std::vector<std::string> leaves;
  gather_leaves(expr, leaves);
  for (const auto& v : leaves) std::cout << v << " ";
  std::cout << "\n";   // x y 2
}
```

The order of the output follows pre-order traversal, which for most expression trees matches the left-to-right reading order of the source. A post-order collection would reverse the relationship between operands and operators but preserve the relative order of leaves among themselves.

When leaves must be filtered by tag — collecting identifiers but not numeric literals, for instance — the predicate is applied to the tag as well as to the leaf status.

```cpp
void gather_identifiers(const dsl::ASTNode& n, std::vector<std::string>& out) {
  if (n.is_leaf() && n.tag() == "ident") {
    out.emplace_back(n.value());
    return;
  }
  for (const auto& c : n.children())
    gather_identifiers(c, out);
}
```

The pattern scales to any extraction policy: collect the first leaf per subtree, collect leaves at a specific depth, collect leaves whose value matches a regex. The library provides no built-in collectors because the public accessors make them trivial, and a hand-written collector is easier to specialize than a generalized callback would be.

## Finding a Node by Tag

Searching for the first node matching a tag is a depth-first search that returns as soon as a match is found. Because `ASTNode` is a value type without optional semantics built in, the function returns a pointer — `nullptr` when no match exists.

```cpp
#include "DSLtk.hpp"
#include <iostream>

const dsl::ASTNode* find_first(const dsl::ASTNode& n, std::string_view want) {
  if (n.tag() == want) return &n;
  for (const auto& c : n.children())
    if (auto* p = find_first(c, want)) return p;
  return nullptr;
}

int main() {
  auto t = dsl::node<"block">(
      dsl::node<"assign">(dsl::leaf<"ident">("x"), dsl::leaf<"num">(1)),
      dsl::node<"return">(dsl::leaf<"ident">("x")));
  if (auto* r = find_first(t, "return"))
    std::cout << "found: " << r->dump() << "\n";
  else
    std::cout << "not found\n";
}
```

The returned pointer aliases the node inside the tree and is valid as long as the tree is alive and unmutated. This is the same lifetime guarantee offered by indexing into `children()`: stable until the containing vector is modified.

A variant that collects all matches, rather than the first, uses the same skeleton as the leaf collector but tests `tag()` against the target.

```cpp
void find_all(const dsl::ASTNode& n, std::string_view want,
              std::vector<const dsl::ASTNode*>& out) {
  if (n.tag() == want) out.push_back(&n);
  for (const auto& c : n.children())
    find_all(c, want, out);
}
```

This is the building block for rewrite-rule application: Chapter 13's rules locate candidate nodes by predicate, and a tag-equality test is the simplest such predicate. The free functions shown here are the manual analog of what the rewrite engine automates.

## Computing Tree Depth

Tree depth is the length of the longest root-to-leaf path, with a single node conventionally counted as depth one. The recursion takes the maximum over the children's depths and adds one.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <algorithm>

std::size_t depth(const dsl::ASTNode& n) {
  if (n.is_leaf()) return 1;
  std::size_t deepest = 0;
  for (const auto& c : n.children())
    deepest = std::max(deepest, depth(c));
  return deepest + 1;
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::leaf<"num">(3),
      dsl::node<"add">(
          dsl::leaf<"num">(1),
          dsl::node<"neg">(dsl::leaf<"num">(2))));
  std::cout << "depth=" << depth(t) << "\n";   // depth=4
}
```

Depth is a useful metric for deciding whether a recursive traversal is safe: a tree of depth one hundred is well within stack limits, while a tree of depth one hundred thousand is not. It is also a diagnostic for grammar pathologies — a deeply right-recursive grammar can produce a chain of single-child nodes whose depth equals the input length.

A breadth metric — the maximum number of children at any node — is computed similarly, by taking the maximum of `size()` across all inner nodes. Together, depth and breadth characterize the shape of a tree compactly and are often worth logging during parser development.

## A Generic Visitor Template

The recursive traversals above are each specialized for a particular action. Factoring the walk from the action yields a generic visitor. Because `ASTNode` is a regular type, a function object that takes a `const ASTNode&` and is called on each node suffices.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <functional>

void visit_pre(const dsl::ASTNode& n,
               const std::function<void(const dsl::ASTNode&)>& f) {
  f(n);
  for (const auto& c : n.children())
    visit_pre(c, f);
}

int main() {
  auto t = dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
  int leaves = 0, inner = 0;
  visit_pre(t, [&](const dsl::ASTNode& n) {
    if (n.is_leaf()) ++leaves; else ++inner;
  });
  std::cout << "leaves=" << leaves << " inner=" << inner << "\n";
}
```

Using `std::function` trades a small amount of performance for the ability to capture arbitrarily in a lambda. Where performance matters — and AST traversal over large trees is a place it can — a template parameter is preferable, inlining the callable and avoiding the type erasure.

```cpp
template <typename F>
void visit_pre(const dsl::ASTNode& n, F&& f) {
  f(n);
  for (const auto& c : n.children())
    visit_pre(c, f);
}
```

The template form composes with the rest of the library: the callable can be a rewrite predicate, a diagnostic accumulator, or a code generator. Chapters 13 and 14 build the rewrite engine on exactly this kind of templated visit.

A two-callback variant — one for the pre-visit and one for the post-visit — supports scoped actions. It is no harder to write and is often the right abstraction for pretty-printers and symbol-table builders.

## Pretty-Printing an Indented View

The built-in `dump()` produces a single line. For human consumption an indented multi-line view is more readable, and it is straightforward to build from the public accessors. The renderer carries an indentation level and recurses.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

void pretty(const dsl::ASTNode& n, int indent) {
  std::cout << std::string(indent * 2, ' ');
  if (n.is_leaf()) {
    std::cout << n.tag();
    if (!n.value().empty()) std::cout << " " << n.value();
    std::cout << "\n";
    return;
  }
  std::cout << n.tag() << ":\n";
  for (const auto& c : n.children())
    pretty(c, indent + 1);
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::leaf<"num">(3),
      dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  pretty(t, 0);
}
```

The output uses two spaces per level and a colon to mark an inner node, distinguishing it from a leaf without relying on parentheses. Any such convention is application-specific; the library intentionally provides only the canonical S-expression and leaves presentation to the user.

A more elaborate renderer might align columns, colorize tags, or annotate each node with its source span if the parser attaches one. None of this requires library support; the tag, value, and children expose everything that was put into the node at construction time.

## An S-Expression Builder

Because `dump()` already emits S-expressions, a custom S-expression builder is rarely necessary. When it is — to insert comments, to quote values differently, or to limit depth — the same skeleton as `dump_impl` applies, written against the public API.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <sstream>

void sexpr(const dsl::ASTNode& n, std::ostringstream& os, int depth_limit) {
  if (depth_limit == 0) { os << "..."; return; }
  os << '(' << n.tag();
  if (n.is_leaf()) {
    if (!n.value().empty()) os << ' ' << n.value();
  } else {
    for (const auto& c : n.children()) {
      os << ' ';
      sexpr(c, os, depth_limit - 1);
    }
  }
  os << ')';
}

int main() {
  auto t = dsl::node<"add">(
      dsl::leaf<"num">(1),
      dsl::node<"mul">(dsl::leaf<"num">(2), dsl::leaf<"num">(3)));
  std::ostringstream os;
  sexpr(t, os, 2);
  std::cout << os.str() << "\n";   // (add (num 1) ...)
}
```

The depth limit truncates recursion and emits `...` for elided subtrees, which is useful when logging a fragment of a large tree. The library's own `dump()` has no such limit because it is intended for complete, canonical output.

This example also illustrates how the private `dump_impl` relates to the public surface: every behavior of `dump()` is reproducible from `tag()`, `is_leaf()`, `value()`, and `children()`. The member exists for convenience and canonical formatting, not because it exposes anything inaccessible otherwise.

## Evaluating a Numeric Expression Tree

A post-order walk is the natural shape of an evaluator. For a tree whose inner nodes are arithmetic operators and whose leaves are numeric literals, the evaluator recurses into the children, obtains their values, and applies the operator.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <string>

long eval(const dsl::ASTNode& n) {
  if (n.is_leaf())
    return std::stol(n.value());
  if (n.size() == 2) {
    long l = eval(n.child(0));
    long r = eval(n.child(1));
    if (n.tag() == "add") return l + r;
    if (n.tag() == "sub") return l - r;
    if (n.tag() == "mul") return l * r;
    if (n.tag() == "div") return r != 0 ? l / r : 0;
  }
  if (n.size() == 1 && n.tag() == "neg")
    return -eval(n.child(0));
  return 0;
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::leaf<"num">(3),
      dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  std::cout << eval(t) << "\n";   // 9
}
```

The evaluator is strict — it computes children before parents — and total, returning zero for unrecognized shapes. A production evaluator would replace the fallback with an error, likely using the `Result<T,E>` type covered in Chapter 22.

This pattern generalizes to any bottom-up attribution: type synthesis, constant folding, abstract-interpretation domains. Each is a post-order walk that synthesizes a node's attribute from its children's attributes. The library's expression-template and fused-evaluation features, covered in Chapters 15 and 16, provide specialized machinery for the evaluation case, but the manual evaluator above remains the clearest exposition of the principle.

## Listing All Identifiers

A slightly richer query collects the values of all leaves whose tag is `ident`, deduplicated and sorted. This is the kind of analysis a compiler front end performs to build a use table.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <set>
#include <string>

void collect_idents(const dsl::ASTNode& n, std::set<std::string>& out) {
  if (n.is_leaf() && n.tag() == "ident") {
    out.insert(n.value());
    return;
  }
  for (const auto& c : n.children())
    collect_idents(c, out);
}

int main() {
  auto prog = dsl::node<"block">(
      dsl::node<"assign">(dsl::leaf<"ident">("x"),
                          dsl::node<"add">(dsl::leaf<"ident">("y"),
                                           dsl::leaf<"num">(1))),
      dsl::node<"return">(dsl::leaf<"ident">("x")));
  std::set<std::string> ids;
  collect_idents(prog, ids);
  for (const auto& s : ids) std::cout << s << "\n";   // x \n y
}
```

Using a `std::set` gives sorted, unique identifiers directly. A `std::vector` with a post-pass `std::sort` and `std::unique` would preserve first-occurrence order if that is preferred. The choice of container is independent of the traversal, which is one reason the library does not mandate a collection style.

The same skeleton, with the predicate changed, collects any class of node: string literals, numeric constants, calls to a particular function. The cost is linear in the tree size plus the cost of inserting into the container, which is logarithmic per element for a `set` and amortized constant for a `vector`.

## Mutating a Child In Place

The mutable overload of `children()` and the mutable `child(i)` permit structural edits. The simplest is replacing a child by assignment.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  dsl::ASTNode t = dsl::node<"add">(
      dsl::leaf<"num">(1),
      dsl::leaf<"num">(2));
  // Replace the right operand with 40.
  t.children()[1] = dsl::leaf<"num">(40);
  std::cout << t.dump() << "\n";   // (add (num 1) (num 40))
}
```

Assignment through `children()[i]` invokes the move-assignment of `ASTNode`, which is cheap: it swaps the internal `string` and `vector` members. The siblings of the replaced child are unaffected, because the vector's storage is not reallocated when an element is assigned in place.

Appending a child is equally direct.

```cpp
dsl::ASTNode list = dsl::node<"seq">(dsl::leaf<"item">("a"));
list.children().push_back(dsl::leaf<"item">("b"));
list.children().push_back(dsl::leaf<"item">("c"));
// list.dump() => (seq (item a) (item b) (item c))
```

The `push_back` may reallocate the vector, invalidating references into earlier children. This is the standard `std::vector` hazard and is covered explicitly under Pitfalls.

## Building a Transformed Copy

A pure functional transformation does not mutate its input; it builds a new tree. This is the safest style when the original tree must be preserved — for example, when a transformation is speculative and may be discarded.

```cpp
#include "DSLtk.hpp"
#include <iostream>

dsl::ASTNode negate_all(const dsl::ASTNode& n) {
  if (n.is_leaf())
    return dsl::node<"neg">(n);          // wrap each leaf in (neg ...)
  std::vector<dsl::ASTNode> kids;
  kids.reserve(n.size());
  for (const auto& c : n.children())
    kids.push_back(negate_all(c));
  return dsl::ASTNode(n.tag(), std::move(kids));
}

int main() {
  auto t = dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
  auto t2 = negate_all(t);
  std::cout << t.dump() << "\n";    // (add (num 1) (num 2))
  std::cout << t2.dump() << "\n";   // (add (neg (num 1)) (neg (num 2)))
}
```

The function recurses, building new inner nodes whose children are the transformed copies of the original children. Leaves are wrapped in a `neg` node, which is constructed by calling `dsl::node<"neg">` with the original leaf as its single child; the leaf is copied because the parameter is `const ASTNode&`.

This pattern is exactly what the rewrite engine generalizes. A rewrite rule, as introduced in Chapter 13, pairs a predicate — which identifies nodes to transform — with a builder that produces the replacement. Applied across a tree, rules compose into the fixpoint optimization machinery of Chapter 14. The manual transformer above is the explicit form of the same idea.

## Mutating During Traversal: Hazards and Discipline

Combining mutation with iteration over the same `children()` vector requires care. The `std::vector` invalidation rules apply in full: `push_back` may reallocate and invalidate all references, iterators, and pointers into the vector; `erase` invalidates from the point of erasure onward.

The safe discipline is to avoid mutating the vector being iterated. When an algorithm must insert or remove children, it should either build a fresh vector — as `negate_all` does — or iterate by index, taking care to adjust the index after structural changes.

```cpp
#include "DSLtk.hpp"
#include <iostream>

// Remove every leaf whose value is "0".
void drop_zero(dsl::ASTNode& n) {
  if (n.is_leaf()) return;
  auto& kids = n.children();
  for (std::size_t i = 0; i < kids.size(); ) {
    if (kids[i].is_leaf() && kids[i].value() == "0")
      kids.erase(kids.begin() + i);   // do not advance i
    else
      drop_zero(kids[i++]);           // recurse, then advance
  }
}

int main() {
  dsl::ASTNode t = dsl::node<"add">(
      dsl::leaf<"num">(0),
      dsl::leaf<"num">(5),
      dsl::leaf<"num">(0));
  drop_zero(t);
  std::cout << t.dump() << "\n";   // (add (num 5))
}
```

The index is advanced only when no erasure occurs, so that the element shifted into the erased position is examined on the next iteration. This is the standard vector-erase idiom and applies unchanged to `ASTNode`'s children.

A range-based `for` loop over `children()` while simultaneously calling `push_back` on the same vector is undefined behavior and must be avoided. When an algorithm genuinely needs to append while iterating, it should iterate over a copy of the vector or over indices.

## Lifetime of References into Children

References and pointers obtained from `children()` and `child(i)` are valid for the lifetime of the parent node, provided the parent's child vector is not mutated. A `const ASTNode&` parameter therefore guarantees that pointers collected during a traversal remain valid for the duration of the call.

When the parent is non-const, mutation may invalidate earlier pointers. The `find_first` function above returns a `const dsl::ASTNode*` that is safe to use as long as the tree is not mutated between the search and the dereference. Code that caches such pointers across a mutation must refresh them.

Copying an `ASTNode` copies its entire subtree, so returning a node by value from a transformation is safe but potentially expensive. Move semantics mitigate this: the transformed-copy pattern moves the new children vector into the constructed node, avoiding a deep copy at the return. For trees of moderate size the cost is acceptable; for very large trees, an arena allocator wrapped around `ASTNode` storage would be the next optimization, though the library does not currently parameterize the allocator.

## Bridging to the Rewrite Feature

The transformations shown in this chapter are written by hand. Chapter 13 introduces the `dsl::rule<Tag>` type, which encapsulates a predicate and a builder, and Chapter 14's `dsl::rewrite_set` composes multiple rules and applies them to fixpoint. The example distributed with the library — `examples/10-rewrite-basics.cpp` — shows the bridge concisely.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main() {
  auto rules = dsl::rewrite_set(
      dsl::rule<"double-neg">(
          [](const dsl::ASTNode& n) {
            return !n.is_leaf() && n.tag() == "neg" && n.size() == 1
                && !n.child(0).is_leaf() && n.child(0).tag() == "neg";
          },
          [](const dsl::ASTNode& n) { return n.child(0).child(0); }));
  auto in = dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"num">(4)));
  std::cout << rules.apply(in).dump() << "\n";   // (num 4)
}
```

The rule's predicate is exactly the kind of structural test this chapter has shown — `is_leaf()`, `tag()`, `size()`, `child(i)` — and its builder returns a new `ASTNode` by extracting an existing subtree. The rewrite engine automates the traversal that a manual transformer would write, applying the rule at every node and rebuilding the tree accordingly.

Understanding the manual traversals in this chapter is therefore a prerequisite for reading the rewrite engine. The engine is not a different kind of computation; it is a scheduling discipline over the same recursive walks, with the predicate and builder factored out of the recursion and supplied as rule parameters.

## Composing Traversal with Diagnostics

Traversal is also the substrate for error reporting. A semantic check walks the tree, and on finding an inconsistency, emits a diagnostic. The `Maybe<T>` and `Result<T,E>` types of Chapters 19 and 22 integrate naturally: a traversal returns a `Result` that is OK on success and error on the first failure.

```cpp
#include "DSLtk.hpp"
#include <iostream>

bool check_div_by_zero(const dsl::ASTNode& n) {
  if (n.is_leaf()) return true;
  if (n.tag() == "div" && n.size() == 2
      && n.child(1).is_leaf() && n.child(1).value() == "0")
    return false;
  for (const auto& c : n.children())
    if (!check_div_by_zero(c)) return false;
  return true;
}

int main() {
  auto t = dsl::node<"div">(
      dsl::leaf<"num">(1),
      dsl::leaf<"num">(0));
  std::cout << (check_div_by_zero(t) ? "ok" : "division by zero") << "\n";
}
```

The check is a pre-order walk that short-circuits on the first offending node. More elaborate checks collect all violations rather than stopping at the first, mirroring the `find_all` pattern. Either style composes with the diagnostic machinery of Chapter 16, which attaches source positions and messages to nodes; the traversal logic itself is unchanged.

## A Worked Example: Printing an Arithmetic Tree

Combining the pieces, a small program builds an arithmetic tree, prints it in three forms, evaluates it, and reports its depth and node count.

```cpp
#include "DSLtk.hpp"
#include <iostream>
#include <algorithm>

std::size_t count(const dsl::ASTNode& n) {
  std::size_t c = 1;
  for (const auto& ch : n.children()) c += count(ch);
  return c;
}

std::size_t depth(const dsl::ASTNode& n) {
  if (n.is_leaf()) return 1;
  std::size_t d = 0;
  for (const auto& ch : n.children()) d = std::max(d, depth(ch));
  return d + 1;
}

long evaluate(const dsl::ASTNode& n) {
  if (n.is_leaf()) return std::stol(n.value());
  long l = evaluate(n.child(0));
  long r = evaluate(n.child(1));
  if (n.tag() == "add") return l + r;
  if (n.tag() == "mul") return l * r;
  return 0;
}

int main() {
  auto t = dsl::node<"mul">(
      dsl::node<"add">(dsl::leaf<"num">(2), dsl::leaf<"num">(3)),
      dsl::node<"add">(dsl::leaf<"num">(4), dsl::leaf<"num">(5)));
  std::cout << "sexpr:   " << t.dump() << "\n";
  std::cout << "nodes:   " << count(t) << "\n";
  std::cout << "depth:   " << depth(t) << "\n";
  std::cout << "value:   " << evaluate(t) << "\n";   // (2+3)*(4+5)=45
}
```

The three queries — count, depth, evaluate — are independent and could be fused into a single walk that returns a struct. For clarity they are kept separate here; in a performance-sensitive context, fusing them into one pass halves the traversal overhead.

## Pitfalls

A few hazards recur often enough to deserve explicit mention. None is a defect in the library; all are consequences of the value-type design and the use of `std::vector` for child storage.

Deep recursion on degenerate trees can overflow the call stack. A grammar that produces a left- or right-chained tree — for example, a list parsed as nested binary `cons` nodes — yields a tree whose depth equals the chain length. For inputs of a few thousand elements this is harmless; for inputs of a million it is not. The remedy is either to rebalance the grammar (parse lists as a single multi-child node) or to convert the recursive walk into an explicit-stack iteration. The library does not impose a recursion strategy, leaving this decision to the application.

Mutating the `children()` vector while iterating it is undefined behavior for the same reasons it is on any `std::vector`. Range-based `for` loops hold an iterator that is invalidated by `push_back` and `erase`. The safe patterns are either to build a fresh vector — the transformed-copy style — or to iterate by index with explicit adjustment, as shown in the `drop_zero` example.

Lifetime of references obtained from `child(i)` or `children()` ends when the parent's child vector is mutated or the parent is destroyed. Returning a pointer into a temporary `ASTNode` is a use-after-free. The transformed-copy pattern avoids this by returning a value, not a reference.

Calling `value()` on an inner node returns the empty string, not a meaningful payload. Code that reads `value()` without first testing `is_leaf()` will silently receive empty strings for every inner node. The `is_leaf()` predicate is the canonical discriminator and should gate every access to `value()`.

Comparing `tag()` against a misspelled literal is a silent no-op. Because tags are runtime `string_view`s, the compiler cannot check that the literal matches a `FixedString` used at construction. A unit test that asserts `dump()` output for a representative tree catches such typos cheaply.

## Summary

Traversing a `dsl::ASTNode` tree is ordinary recursive programming over an ordinary value type. The ten public accessors — `tag()`, `is_leaf()`, `value()`, `children()`, `child(i)`, `size()`, `dump()`, and `operator==` — are sufficient for every walk, query, and transformation shown in this chapter. Pre-order and post-order recursions cover the two canonical action orderings; indexed loops cover the case where child position matters.

The built-in `dump()` member emits a canonical single-line S-expression suitable for logging and assertions. Richer rendering — indented, depth-limited, annotated — is built from the public accessors in a few lines of application code, because `dump_impl` exposes no behavior that the public surface does not.

Queries such as node counting, leaf collection, tag search, and depth measurement are one-liner recursions. Mutations — replacing, appending, or removing children — use the mutable `children()` overload and obey standard `std::vector` invalidation rules. The transformed-copy pattern builds a new tree from an old one without mutating the input, and is the explicit form of what the rewrite engine of Chapters 13 and 14 automates.

The hazards are the standard hazards of recursive tree programming: stack depth on degenerate shapes, vector invalidation during mutation, and the discipline of testing `is_leaf()` before reading `value()`. None of these is specific to DSLtk; all are consequences of a deliberately plain, value-oriented design that keeps trees inspectable, copyable, and easy to reason about.
