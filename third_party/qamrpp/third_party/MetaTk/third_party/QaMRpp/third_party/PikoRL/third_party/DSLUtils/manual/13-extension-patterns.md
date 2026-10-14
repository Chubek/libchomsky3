# Chapter 13: Extension Patterns

## Compose only needed features

Avoid a catch-all DSL type when multiple subsystems have different needs.

## Domain wrappers

Wrap low-level facilities with domain names:

- grammar functions returning `Parser<T>`
- AST builders returning tagged nodes
- rewrite bundles per optimization phase

## Safe extension points

- add new parser helpers around `parser(...)`
- add new rewrite rules via `rewrite_set(...)`
- add custom literal suffixes with `literal_set(...)`

Future work ideas (not in current API): typed AST payload variants and richer parser error spans.
