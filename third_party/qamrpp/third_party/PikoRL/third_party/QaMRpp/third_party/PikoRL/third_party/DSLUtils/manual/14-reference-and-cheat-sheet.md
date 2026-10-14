# Chapter 14: Reference and Cheat Sheet

## Core factories

- `predicate(fn)`, `pipe(fn)`
- `when<Key>(fn)`, `otherwise(fn)`, `match(...)`
- `leaf<Tag>(v)`, `node<Tag>(...)`
- `rule<Tag>(pred, trans)`, `rewrite_set(...)`
- `lit<Suffix>(fn)`, `literal_set(...)`
- `parser(fn)`, `optional(p)`, `try_parse(p)`, `labeled(p, label)`

## Common types

- `ASTNode`
- `Maybe<T>`
- `Result<T,E>`
- `Lazy<T>`
- `Parser<T>`, `ExpectedResult<T>`, `ParseOutcome<T>`
- `Task`, `TaskChain`, `TaskState`

## Minimal end-to-end flow

1. Parse input to AST
2. Rewrite AST to simplified form
3. Evaluate or emit output

Use the `examples/` directory for concrete code for each step.
