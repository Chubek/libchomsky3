Fuzz Mutation and Corpus
========================

Subjects covered
----------------

1. Byte generation strategy.
2. Printable text generation strategy.
3. Bit-flip mutation operation.
4. Byte-set mutation operation.
5. Byte-insert mutation operation.
6. Byte-delete mutation operation.
7. Range-duplicate mutation operation.
8. Text-overwrite mutation operation.
9. Mutation count randomization.
10. Corpus seed curation practices.

Safety requirements
-------------------

- Guard all size arithmetic against overflow.
- Bound all mutation output by configured max size.
- Ensure memmove/memcpy ranges stay valid after resize.
