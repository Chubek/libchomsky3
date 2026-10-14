# Chapter 6 - Error Handling and Diagnostics

Runtime and parser failures throw `zethamem::Error` with direct human-readable messages.

Common diagnostics include:
- duplicate column
- unknown column
- unknown table
- type mismatch
- malformed schema/query syntax
- trailing garbage in query/schema input

This policy prioritizes clear failure behavior over permissive parsing.
