# Chapter 7 - Testing and Fuzzing Strategy

Test assets live in `tests/`:
- Catch2 unit tests for schema, query, runtime behavior, and edge cases
- AFL++ fuzz harnesses for parser and query execution surfaces

The suite targets the must-test matrix from AGENTS.md: malformed input, predicate behavior, CRUD flow, and type validation.
