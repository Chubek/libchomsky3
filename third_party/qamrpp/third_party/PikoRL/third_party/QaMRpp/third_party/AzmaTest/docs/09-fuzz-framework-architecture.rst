Fuzz Framework Architecture
===========================

Subjects covered
----------------

1. Header-only fuzz helper scope.
2. Target callback contract.
3. Option structure and defaults.
4. Statistics model and counters.
5. Deterministic RNG design.
6. Working buffer allocation strategy.
7. Corpus replay phase.
8. Generation/mutation phase.
9. Failure handling and stop-on-failure behavior.
10. Status-code based completion semantics.

Design limits
-------------

- Framework is a utility layer, not a full fuzz engine.
- Corpus directory walking belongs in test-side code.
