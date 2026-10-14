IDL Lexical Model
=================

Subjects covered
----------------

1. Whitespace classes recognized by the tokenizer.
2. Line comment handling and end-of-line semantics.
3. String literal boundaries and escape processing.
4. Identifier character classes and naming limits.
5. Numeric literal forms currently accepted.
6. Delimiter tokens and punctuation categories.
7. Source offset tracking during token consumption.
8. Line and column updates for diagnostics.
9. Invalid byte handling strategy.
10. Lexer behavior on truncated input.

Guidance
--------

- Keep lexical rules strict and explicit.
- Avoid parser-side reinterpretation of token bytes.
- Preserve raw spans for diagnostics and debug output.
