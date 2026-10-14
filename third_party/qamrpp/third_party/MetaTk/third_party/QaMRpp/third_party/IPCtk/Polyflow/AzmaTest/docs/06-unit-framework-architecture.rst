Unit Framework Architecture
===========================

Subjects covered
----------------

1. Header-only design goals.
2. Static registry model and limits.
3. Test function signature conventions.
4. Registration mechanics and portability caveats.
5. Execution context lifecycle.
6. Failure capture model.
7. Long-jump control flow behavior.
8. Assertion accounting strategy.
9. Filtered execution model.
10. Summary and exit status contract.

Hardening checklist
-------------------

- Keep assertion macros single-evaluation.
- Keep framework behavior deterministic across compilers.
- Preserve strict compile compatibility under warning-as-error builds.
