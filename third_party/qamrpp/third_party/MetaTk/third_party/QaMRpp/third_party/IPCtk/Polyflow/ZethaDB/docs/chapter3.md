# Chapter 3 - Runtime and Storage

`Table` owns a schema and row vector. `Database` owns table map.

Execution paths:
- insert validates full row assignment and types
- select returns matching rows and selected metadata
- update validates assigned column values
- delete removes matching rows

Predicates evaluate through typed comparison with explicit numeric cross-compare support.
