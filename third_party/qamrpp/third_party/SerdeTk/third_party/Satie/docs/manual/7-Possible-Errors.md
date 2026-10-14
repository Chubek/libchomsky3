# Possible Errors

## Parse-time errors

### `satie::ParseError`

Thrown by `parse_cnf`, `parse_dimacs`, and `parse_auto` delegated parser paths.

Diagnostic payload:

- `line`
- `column`
- `message`

Canonical causes:

- malformed DIMACS problem line (`p cnf ...`);
- duplicate DIMACS problem line;
- literal outside `int32` range;
- literal index exceeding declared DIMACS variable count;
- DIMACS clause count mismatch;
- unterminated DIMACS clause missing trailing `0`;
- invalid CNF DSL token;
- CNF DSL empty clause expression.

## I/O errors

### `std::runtime_error`

Thrown by file-based entrypoints:

- `parse_dimacs_file(path)`;
- `parse_auto_file(path)`.

Cause: file open failure or inaccessible path.

## Runtime/algorithmic errors

### `std::overflow_error`

Thrown by `NaiveSolver::count_models()` when variable count is outside guarded counting range.

## Semantic pitfalls

Not exceptions, but high-risk misuse patterns:

- relying on CNF DSL variable numbering externally without stable symbol table export;
- comparing raw statistics across different engines without normalization;
- using Native solver for large formulas;
- supplying DIMACS with inconsistent header metadata.

## Defensive integration pattern

- parse once at ingestion boundary;
- catch `ParseError` and persist `(line,column,message)`;
- reject or quarantine malformed instances;
- export normalized DIMACS for reproducible downstream runs;
- use `Engine::CDCL` for primary solve path;
- reserve Native/DPLL for diagnostics and differential testing.
