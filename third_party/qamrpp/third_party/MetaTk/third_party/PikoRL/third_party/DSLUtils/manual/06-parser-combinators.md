# Chapter 6: Parser Combinators

Core parser pieces:

- `dsl::ParsecInput`
- `dsl::ExpectedResult<T>`
- `dsl::Parser<T>`
- `dsl::parser(fn)`

Combinators:

- Sequence: `a & b`
- Choice: `a | b`
- Repetition: `*p`
- Optional: `optional(p)`
- Backtracking helper: `try_parse(p)`
- Labels: `labeled(p, "name")`

Helpers:

- `ch(c)`
- `satisfy(pred, label)`
- `run_parser(p, source)`

`ParseFailureKind::Committed` is used to block fallback branches after consuming input; this is key to predictable error behavior.
