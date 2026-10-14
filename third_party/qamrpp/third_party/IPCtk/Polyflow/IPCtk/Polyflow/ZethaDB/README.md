# ZethaDB

ZethaDB is a header-only, in-memory, schema-based database for C++.

## Goals

- header-only API surface
- explicit schemas and runtime type validation
- tiny query DSL for insert/select/update/delete
- parser and execution as separate phases
- parser integration through `DSLUtils.hpp` combinators using optional-style parser results

## Non-goals

ZethaDB does not provide persistence, memory mapping, transactions, distributed runtime, or SQL-complete features.

## Quick example

```cpp
#include "ZethaDB.hpp"

int main() {
    zethadb::Database db;
    zethadb::exec_schema(db, "table users { id: int; name: string; active: bool; }");
    zethadb::exec_query(db, "insert users { id: 1, name: \"alice\", active: true }");
    auto result = zethadb::exec_query(db, "select users where id == 1");
    return result.rows.empty();
}
```

## Development layout

- `ZethaDB.hpp`: core model, runtime engine, AST, and parser entry points
- `DSLUtils.hpp`: parser combinator toolkit used by parser layer
- `tests/`: Catch2 unit tests and AFL++ fuzz targets
- `docs/`: chaptered documentation and build make targets
