# DSLtk — A Manual

This directory contains the complete twenty-eight-chapter manual for **DSLtk**,
the header-only Domain-Specific Language construction toolkit for modern C++
(C++20). The library is distributed as a single header, `DSLtk.hpp`, and the
manual documents every public type, free function, feature tag, and helper it
exposes.

The manual is organised into twelve parts that progress from foundational
concepts through to the most advanced subsystems. Readers new to the library
should begin with Chapter 1 and read sequentially; experienced readers may use
the table of contents below to jump directly to the feature they need.

## Table of Contents

### Part I — Foundations

- [Chapter 1 — Introduction and Design Philosophy](ch01-introduction.md)
- [Chapter 2 — Getting Started: Installation, Build, and First Program](ch02-getting-started.md)
- [Chapter 3 — The CRTP Foundation: `dsl::DSL<Derived, Features...>`](ch03-crtp-foundation.md)
- [Chapter 4 — `FixedString`: Compile-Time Strings as Template Parameters](ch04-fixed-string.md)
- [Chapter 5 — Feature Tags, Mixins, and Utility Concepts](ch05-features-and-concepts.md)

### Part II — Data Flow

- [Chapter 6 — The Pipeline Feature](ch06-pipeline.md)
- [Chapter 7 — The Operators Feature: Predicate Composition](ch07-operators.md)

### Part III — Pattern Matching

- [Chapter 8 — Pattern Matching with `match`, `when`, and `otherwise`](ch08-pattern-matching.md)
- [Chapter 9 — The `pattern<>` Compile-Time Regex Engine](ch09-pattern-regex.md)

### Part IV — Abstract Syntax Trees

- [Chapter 10 — The `ASTNode` Value Type](ch10-ast-node.md)
- [Chapter 11 — Building Trees with `leaf<>` and `node<>`](ch11-leaf-and-node.md)
- [Chapter 12 — Traversing and Dumping ASTs](ch12-ast-traversal.md)

### Part V — Rewriting

- [Chapter 13 — The Rewrite Feature: Rules and Predicates](ch13-rewrite-rules.md)
- [Chapter 14 — Rewrite Sets and Fixpoint Optimization](ch14-rewrite-sets.md)

### Part VI — Expression Templates

- [Chapter 15 — Expression Templates: Lazy Trees](ch15-expr-templates.md)
- [Chapter 16 — `BinExpr`, `UnaryExpr`, and Fused Evaluation](ch16-binexpr-evaluation.md)

### Part VII — Customization

- [Chapter 17 — Custom Literals: `lit`, `literal_set`, `parse_literal`](ch17-custom-literals.md)

### Part VIII — Caching and Laziness

- [Chapter 18 — Memoization: `memoize` and `MemoizedCallable`](ch18-memoization.md)
- [Chapter 19 — `Lazy<T>`: Deferred Single-Shot Values](ch19-lazy.md)

### Part IX — Functional Types

- [Chapter 20 — The `Maybe<T>` Type and Monadic Operations](ch20-maybe.md)
- [Chapter 21 — Monadic Helpers for `std::optional`](ch21-optional-helpers.md)
- [Chapter 22 — The `Result<T,E>` Type and Error Flow](ch22-result.md)

### Part X — Parser Combinators

- [Chapter 23 — Parser Combinators: `ParsecInput`, `ExpectedResult`, `Parser`](ch23-parser-core.md)
- [Chapter 24 — Primitive Parsers and Basic Combinators](ch24-primitive-parsers.md)
- [Chapter 25 — Parser Diagnostics, Semantic Actions, and `run_parser`](ch25-parser-diagnostics.md)

### Part XI — Task Pipelines

- [Chapter 26 — Task Pipelines: `Task`, `TaskChain`, `TaskState`](ch26-task-pipeline.md)

### Part XII — Parsing Expression Grammars

- [Chapter 27 — The PEG Definition Grammar: Rules and Channels](ch27-peg-definition.md)
- [Chapter 28 — PEG Combinators, `PEGMatch`, and the `PEGMatcher`](ch28-peg-combinators-matcher.md)

## Building the Manual

The manual is authored in GitHub-Flavored Markdown, one file per chapter, so
that it is readable directly in any text editor or source repository. A build
script named `build.sh` is intended to live alongside this `README.md` and
compiles the whole manual into four output formats:

- **HTML** — a single navigable web page (or one page per chapter).
- **PDF** — a typeset, printable document.
- **LaTeX** — the intermediate LaTeX source, suitable for further typesetting.
- **XML** — a structured representation for tooling and indexing.

Each format is produced from the same Markdown sources, so the prose, code
listings, and cross-references remain identical across every output. The
canonical toolchain for the conversion is [Pandoc](https://pandoc.org/) with a
LaTeX engine (such as `xelatex`) backing the PDF output.

## Conventions Used in This Manual

Code listings are presented in fenced blocks and use the actual `dsl::` API.
Every example is written to be compilable against `DSLtk.hpp` with a C++20
compiler. Throughout the text, the names of types and functions are set in
backticks (for example, `dsl::DSL`, `dsl::leaf`), and chapter cross-references
appear as “Chapter 6, *The Pipeline Feature*”.

The manual describes the library as implemented; it is a user's guide, not a
change log or a critique. Behaviour is documented faithfully against the
header, and design notes explain *why* a feature works the way it does rather
than how it might be changed.
