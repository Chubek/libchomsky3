# Predicate-heavy Example

This example intentionally uses **implicit** start-symbol selection.

- No `@parser:start(...)` directive is present.
- Start symbol is the first user-defined non-terminal: `start`.

Build parser:

`parzek compile predicates.pzg --create-visitor=PredicateVisitor.hpp`
