# Chapter 5 - Parser Design with DSLUtils

ZethaMEM uses `DSLUtils.hpp` parser primitives (`dsl::ParsecInput`, `dsl::Parser<T>`, `dsl::parser`) with `std::optional<T>` parse results and composes small token parsers:
- whitespace skipper
- keyword parser
- symbol parser
- identifier parser
- literal parsers
- predicate parser

Leaf parsers are intentionally small and reusable (`symbol`, `identifier`, `keyword`, and literal parsers). Composed parsers (`value_literal`, `comparison_op`, `predicate`) build on those lexical units with declarative choice/sequence flow and minimal manual state wiring.

Parser success/failure is represented as "value or no value" (`std::optional`) rather than structured parse-error objects. Human-readable diagnostics remain a higher-level concern in schema/query entry points, which throw `zethamem::Error` with context-specific messages.

Schema and query parse functions orchestrate these combinators and then build AST values.
