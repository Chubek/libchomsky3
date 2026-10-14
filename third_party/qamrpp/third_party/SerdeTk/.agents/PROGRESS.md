2026-07-13

- Read `.agents/AGENTS.md` and standards in `.agents/docs`.
- Baseline sample execution: all three JSON and XML samples parse; both YAML samples fail indentation handling; all three S-expression files are empty and incorrectly parse as an empty root array.
- Found parser defects: YAML assumes fixed two-space nesting and lacks flow/quoted/block scalar support; XML drops attributes, entities, and text-node structure; S-expression accepts unbalanced delimiters and empty input; JSON accepts non-JSON whitespace/control characters and lone UTF-16 surrogates.
- Build integration is independently broken: `include/SerdeTk.hpp` requires the unavailable `dsl::*` API while the repository `DSLtk.hpp` is a placeholder. Parser tests use a temporary `DSLUtils.hpp` include shim; no dependency files modified.
- Replaced the JSON, YAML, XML, and S-expression parser stubs in `include/SerdeTk.hpp` with strict parsers. JSON now rejects unescaped control characters, non-JSON whitespace, and lone UTF-16 surrogates. YAML now handles variable indentation, flow collections, quoted scalars, block scalars, directives/document markers, and rejects tab-indented structure. XML now preserves attributes as `@name`, mixed text as `#text`, decodes entities/CDATA, rejects mismatched/multiple roots, and emits attributes/text correctly. S-expression parsing now rejects empty input and unbalanced lists, supports comments and quoted strings.
- Populated the previously empty `samples/S-Expr/*.sexp` files with valid non-empty documents.
- Added focused parser regressions in `tests/SerdeTk/unit/test_parser_workflow.cpp` for invalid JSON control/surrogate cases, YAML flow and block scalars plus tab rejection, XML attribute/entity/mismatched-root cases, and S-expression comment/balance/empty-input cases.
- Verification:
  - `tests/SerdeTk/unit/test_parser_workflow.cpp` builds and passes with a temporary `DSLUtils.hpp` shim substituting for the missing `dsl::*` dependency surface.
  - Sample corpus status: all 3 JSON, all 3 XML, both YAML, and all 3 S-expression files parse successfully.
- Remaining external issue: the repository-local `DSLtk.hpp` remains a placeholder and prevents a native build without the temporary shim; parser behavior is verified independently of that integration defect.
