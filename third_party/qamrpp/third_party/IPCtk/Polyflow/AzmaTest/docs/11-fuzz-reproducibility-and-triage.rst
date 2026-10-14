Fuzz Reproducibility and Triage
===============================

Subjects covered
----------------

1. Seed selection and recording policy.
2. Deterministic replay workflow.
3. Capturing failing bytes.
4. Mapping status codes to bug classes.
5. Distinguishing parser errors from harness faults.
6. Minimization strategy for failing inputs.
7. Regression corpus promotion flow.
8. Logging controls for noisy runs.
9. CI vs local fuzz profile differences.
10. Crash triage ownership across parser and framework.

Recommended loop
----------------

- Reproduce with fixed seed.
- Save failing payload.
- Reduce input while preserving failure.
- Add reduced case to regression corpus.
- Add unit assertion for repaired behavior.
