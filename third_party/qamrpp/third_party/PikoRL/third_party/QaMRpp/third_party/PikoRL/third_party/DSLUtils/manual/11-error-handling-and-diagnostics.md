# Chapter 11: Error Handling and Diagnostics

## `Result<T,E>`

Use `Result` for explicit success/failure propagation in APIs that should not throw by default.

- `is_ok()`, `is_err()`
- `map`, `and_then`, `map_err`
- `unwrap_or`

## Parser diagnostics

`ParseError` tracks:

- position (`pos`)
- severity (`Soft` vs `Committed`)
- expected labels

Use `labeled()` aggressively so failures are meaningful. For user messages, convert parse positions to line/column in your own layer.
