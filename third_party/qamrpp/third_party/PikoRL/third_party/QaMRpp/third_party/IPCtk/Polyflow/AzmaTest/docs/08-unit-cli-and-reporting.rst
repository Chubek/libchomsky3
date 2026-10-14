Unit CLI and Reporting
======================

Subjects covered
----------------

1. `azma_unit_main` entrypoint behavior.
2. `--filter` argument parsing forms.
3. Help output contract.
4. Test begin/pass/fail line formats.
5. Failure context fields (file, line, expression, note).
6. Summary counters.
7. Exit code conventions.
8. No-match filter behavior.
9. CI integration expectations.
10. Machine-readable output extension points.

CI usage
--------

- Run full suite in default mode.
- Run focused suites by filter for fast regression loops.
- Fail pipeline on nonzero exit.
