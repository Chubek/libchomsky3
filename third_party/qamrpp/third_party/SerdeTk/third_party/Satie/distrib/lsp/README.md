# Satie LSP (scaffold)

Reserved for a future Language Server Protocol implementation providing:

- DSL syntax diagnostics (delegates to the CNF parser in `Common.hpp`);
- completion for REPL commands (`:load`, `:solve`, ...) and DSL operators;
- hover docs for engine keywords (`native`, `dpll`, `cdcl`).

## Structure

```
distrib/lsp/
  package.json   — server manifest (to be populated)
  server/        — server entry (to be populated)
```

The parser combinator toolkit in `DSLtk.hpp` is the intended backend for
tokenization and diagnostic emission.
