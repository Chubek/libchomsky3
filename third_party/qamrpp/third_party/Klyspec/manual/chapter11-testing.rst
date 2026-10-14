Chapter 11 - Testing
=====================

Smoke tests live in ``tests/smoke`` and cover:

- registry and alias lookup,
- runtime parser behavior,
- Klytmk parsing success/failure,
- IPC service roundtrip behavior,
- profile loading.

Test style is incremental: implement, compile, run smoke, continue.
