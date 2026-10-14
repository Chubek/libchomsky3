Unit Assertions and Fixtures
============================

Subjects covered
----------------

1. Expectation vs fail-fast assertions.
2. Integer equality assertion behavior.
3. Unsigned width-specific assertions.
4. String comparison semantics with null safety.
5. Pointer equality and inequality checks.
6. Span equality checks for byte-oriented APIs.
7. Failure message formatting standards.
8. Fixture emulation using helper setup/teardown functions.
9. Test naming conventions for filterability.
10. Suggested additions (size_t/bool/negative assertions).

Fixture pattern
---------------

- Build input in helper setup functions.
- Assert parser status and diagnostics.
- Destroy all owned allocations in teardown helpers.
