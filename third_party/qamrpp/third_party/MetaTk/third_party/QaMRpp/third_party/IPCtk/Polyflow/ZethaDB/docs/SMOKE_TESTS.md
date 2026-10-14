# ZethaDB Smoke Tests and CLI Command Reference

## Smoke levels

- Level 0:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build -j`
- Level 1:
  - `cmake --build build --target zethadb_smoke_include -j`
- Level 2:
  - `cmake --build build --target zetha-mem zetha-rdb -j`
  - `/tmp/zetha-mem --help` or `build/zetha-mem help`
  - `/tmp/zetha-rdb --help` or `build/zetha-rdb help`
- Level 3 (Stage 10 E2E):
  - `bash tests/e2e/smoke_stage10.sh`
  - `ctest --test-dir build --output-on-failure -R stage10_e2e`
- Full noexec-safe pipeline profile:
  - `ctest --test-dir build --output-on-failure -L full_pipeline`

## zetha-mem CLI

- Modes:
  - Interactive: `build/zetha-mem`
  - Scripted: `build/zetha-mem --script <file>`
- Script line format:
  - one command per line;
  - whitespace tokenization;
  - supports `'single'` and `"double"` quotes;
  - supports `\` escaping in unquoted/quoted tokens;
  - supports multiline continuation when quote is left open;
  - `#` prefix for comments.
- Commands:
  - `help`
  - `create`
  - `open <path>`
  - `put <path> <k=v>...`
  - `get <path>`
  - `erase <path>`
  - `query <query-text>`
  - `dump <path>`
  - `load <path>`
  - `close`
  - `exit` / `quit`

## zetha-rdb CLI

- Modes:
  - Interactive: `build/zetha-rdb`
  - Scripted: `build/zetha-rdb --script <file>`
- Script line format:
  - one command per line;
  - whitespace tokenization;
  - supports `'single'` and `"double"` quotes;
  - supports `\` escaping in unquoted/quoted tokens;
  - supports multiline continuation when quote is left open;
  - `#` prefix for comments.
- Commands:
  - `help`
  - `create <path> [schema-text]`
  - `open <path>`
  - `schema-load <schema-text>`
  - `schema-load --file <schema-file>`
  - `put <table> <id> <k=v>...`
  - `get <table> <id>`
  - `erase <table> <id>`
  - `query <query-text>`
  - `maintain`
  - `close`
  - `exit` / `quit`

## Exit codes and diagnostics

- `0`: success;
- `2`: command/usage error (unknown command, missing args);
- `10`: parse failure;
- `11`: schema failure;
- `12`: storage failure;
- `13`: IPC/serialization failure;
- `99`: internal "exit" sentinel (interactive/script control, process exits with `0`).

Diagnostics are emitted as:

- `error[parse]: ...`
- `error[schema]: ...`
- `error[storage]: ...`
- `error[ipc]: ...`
- `error[usage]: ...`

Machine-readable diagnostics are emitted to `stderr` as a compact JSON object on the next line:

- `{"kind":"parse|schema|storage|ipc|usage","context":"...","detail":"..."}`
- `{"kind":"usage","message":"..."}`

## Operator runbook

- Parse failures:
  - code `10`, `error[parse]`;
  - action: validate query/schema grammar, verify quoting/escaping in script lines.
- Schema failures:
  - code `11`, `error[schema]`;
  - action: run `schema-load --file <path>` with minimized schema; verify column types and primary key constraints.
- Storage failures:
  - code `12`, `error[storage]`;
  - action: check file path and permissions, run `maintain`, then reopen DB.
- IPC/serde failures:
  - code `13`, `error[ipc]`;
  - action: regenerate dump, verify dump/load file path and non-empty serialized payload.
- Usage failures:
  - code `2`, `error[usage]`;
  - action: run `help`, check required args, close unterminated quotes in script mode.

Recovery workflow:

- `zetha-rdb`: `open <db>`, `maintain`, `query <known-path>`, `schema-load --file <schema>` if schema drift is suspected.
- `zetha-mem`: `open <db>`, `load <dump>`, `query <known-path>`, `dump <new-dump>`.

## Compatibility/constraints matrix

- workspace may be mounted `noexec`; direct `ctest` binary invocation fails for unit executables.
- noexec-safe test execution is provided through `/tmp` copy wrappers for unit tests.
- Stage 10 E2E copies CLI binaries to `/tmp` before execution.
- recommended CI command in this environment:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build -j`
  - `ctest --test-dir build --output-on-failure -L full_pipeline`
