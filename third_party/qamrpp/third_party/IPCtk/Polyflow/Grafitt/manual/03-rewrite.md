# Rewriting

- `rewrite::rule` captures match query and replacement payload shape.
- `rewrite::apply_once()` executes a concrete one-step rewrite for string-vertex graphs when `replacement` is shaped as `lhs->rhs`.
- Current behavior:
  - ensure `lhs` exists
  - add `rhs` if absent
  - remove outgoing edges from `lhs`
  - insert `lhs -> rhs`
