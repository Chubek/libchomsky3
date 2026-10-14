Cross-Substrate Workflows
=========================

Subjects covered
----------------

1. Shared contracts between IDL parser and test substrates.
2. Reusing unit fixtures as fuzz corpus seeds.
3. Promoting fuzz failures to unit tests.
4. Aligning diagnostics assertions across test styles.
5. Smoke-test layering: compile, runtime, focused parser checks.
6. Maintaining API compatibility while improving internals.
7. Using parser debug dumps to aid fuzz triage.
8. Integrating docs updates with behavioral changes.
9. CI execution order for fast feedback.
10. Release readiness checklist across parser, unit, and fuzz.

Execution model
---------------

- Start with compile-only smoke.
- Run core runtime smoke.
- Run focused unit suites.
- Run deterministic fuzz pass.
- Update docs whenever user-facing behavior changes.
