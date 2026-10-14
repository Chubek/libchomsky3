IDL Declarations
================

Subjects covered
----------------

1. Top-level declaration lifecycle.
2. Section declaration structure.
3. Symbol declaration naming constraints.
4. API declaration shape and intent.
5. Nested declaration handling.
6. Duplicate name behavior and future policy hooks.
7. Declaration ordering guarantees.
8. Error recovery at declaration boundaries.
9. Internal representation vs public exposure boundaries.
10. Validation points suitable for unit and fuzz tests.

Practices
---------

- Treat declaration parse failures as local when possible.
- Keep source ranges on every declaration node.
- Avoid leaking backend parser internals into public headers.
