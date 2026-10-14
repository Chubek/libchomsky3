Chapter 11 - Extension DSLs
===========================

DSL-backed extensions use ``mpc-parsec``. Keep four phases separate:

- grammar and parser construction;
- AST ownership;
- semantic validation;
- execution or lowering.

Diagnostics must retain source path, line, column, and violated constraint.
Grammar parsing must not perform registration or runtime mutation. Validate
before lowering. Place examples beside the extension and test malformed input.
