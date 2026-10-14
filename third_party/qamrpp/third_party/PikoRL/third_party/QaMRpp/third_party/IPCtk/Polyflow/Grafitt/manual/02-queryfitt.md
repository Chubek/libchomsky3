# Queryfitt

- `queryfitt::query` models pattern, traversal, path, reachability, and aggregation clauses.
- `queryfitt::parse_text()` now parses the textual form used by `examples/*.qfitt`, including optional `---` metadata blocks and `in "..." { ... }` bodies.
- `queryfitt::execute()` evaluates parsed/native queries:
  - traversal: BFS order (depth-limited)
  - path: shortest path
  - reachable: boolean reachability
  - aggregate: scalar counts (`count vertices`, `count edges`)
  - match: edge-oriented match emission with caller predicates
