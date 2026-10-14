# Parsing

Jiterati has two parser surfaces.

## Textual IR Parser
Implemented entry point:
```cpp
std::optional<TerseModule> parse_terse_module(
    std::string const& source,
    std::vector<Diagnostic>* diagnostics = nullptr);
```

Behavior:
- returns `std::nullopt` on parse failure;
- appends diagnostics when a diagnostics vector is provided;
- produces a `TerseModule` on success;
- accepts the subset implemented by `IR/Parse.cpp`.

## JBL Parser
Implemented entry point:
```cpp
std::unique_ptr<Module> parse_jbl(std::string_view source, std::string* error);
std::string print_jbl(Module const& module);
```

JBL is a minimal printer/parser over the core API model. It is not a full replacement for the textual IR grammar.

## CLI Parsing
```sh
jiterati-cli parse input.ir
```

Current CLI behavior:
- reads the input file;
- invokes textual IR parsing;
- prints normalized confirmation/output or diagnostics;
- exits nonzero on errors.

## Error Handling
Parsing clients should:
- pass a diagnostics vector;
- report all collected diagnostics;
- avoid continuing to validation/backend compilation on parse failure;
- preserve source locations when available.

## Parser Boundaries
- Syntax belongs in `specs/IR.ebnf`.
- Semantics belong in `specs/IR.md` and validation.
- Recovery should not invent IR.
- Unsupported syntax should produce diagnostics, not partial silent success.
