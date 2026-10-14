# Chapter 8: Rewriting

Rewriting is modeled as predicate + transform rules and a fixpoint pass.

```cpp
auto rs = dsl::rewrite_set(
  dsl::rule<"elim-double-neg">(pred, trans)
);
auto out = rs.apply(in);
```

## Behavior details

- Bottom-up traversal: children first, then current node.
- Multiple rules may fire on one node in a pass.
- Iterates until stable or `max_iterations` (default 100).

Design rewrite predicates so they are precise and terminating.
