IDL Diagnostics and Recovery
============================

Subjects covered
----------------

1. Diagnostic object model.
2. Severity levels and status mapping.
3. Message formatting rules.
4. Source range construction from offsets.
5. Line/column accuracy requirements.
6. Parser recovery checkpoints.
7. Fail-fast vs continue behavior.
8. Invalid token recovery messaging.
9. API surface for diagnostic iteration.
10. CLI printing format and exit code mapping.

Operational notes
-----------------

- Diagnostics should prefer precise token context over generic failure text.
- Recovery paths must never produce undefined behavior.
- Invalid input must still allow deterministic teardown.
