IDL Foundations
===============

Subjects covered
----------------

1. Purpose of AzmaIDL in parser and tooling workflows.
2. Document model: source text to parsed document.
3. Stability expectations for public API consumers.
4. Ownership boundaries between caller buffers and parser memory.
5. Determinism guarantees for repeated parse input.
6. Error model overview and status code interpretation.
7. Build-time constraints and portability assumptions.
8. Relationship between language grammar and runtime structures.
9. Current non-goals and unsupported constructs.
10. Migration approach for future parser backends.

Core workflow
-------------

- Prepare immutable input bytes.
- Initialize parser options and diagnostics preferences.
- Parse once per document.
- Inspect root declarations and values.
- Release document resources with matching teardown APIs.

API contracts
-------------

- Callers own input buffers.
- Library owns parse tree allocations.
- Diagnostics are best-effort but deterministic for identical input.
- Invalid arguments return explicit status codes.
