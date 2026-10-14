# Chapter 4 - Query AST

Query parsing produces plain data-only AST nodes:
- `InsertQuery`
- `SelectQuery`
- `UpdateQuery`
- `DeleteQuery`
- `Predicate`

Execution is a separate pass on top of parsed AST nodes. This separation supports deterministic tests, future optimizations, and parser evolution without storage coupling.
