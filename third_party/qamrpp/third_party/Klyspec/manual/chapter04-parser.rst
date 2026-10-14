Chapter 4 - Parser Internals
=============================

The parser uses ``DSLUtils`` combinator primitives:

- token parser,
- identifier parser,
- quoted string parser,
- whitespace skipper.

Diagnostics are reported with:

- byte offsets,
- line and column,
- short error message.

The runtime CLI parser supports:

- short and long options,
- stacked short flags,
- option-value consumption,
- ``--`` end-of-options sentinel,
- positional collection,
- required/default checks.
