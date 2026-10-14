# AzmaIDL v0.1 Specification

## 1. Purpose

AzmaIDL is a declarative language for:
- describing APIs
- defining unit test generation strategies
- defining fuzz harnesses and input schemas
- configuring code generation and execution metadata
- emitting C/C++ test scaffolding targeting:
  - `AzmaUnit.h`
  - `AzmaFuzz.h`

AzmaIDL is **not**:
- a general-purpose programming language
- a template engine
- a replacement for C headers
- a coverage-guided fuzzing engine

AzmaIDL files are parsed into an internal representation, validated, normalized, expanded, and emitted as generated source code or manifests.

---

# 2. Conformance Terms

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are to be interpreted as normative requirements.

An implementation that claims AzmaIDL v0.1 support MUST accept all required constructs in this specification and reject invalid constructs with diagnostics.

---

# 3. File Model

An AzmaIDL source file consists of a sequence of:
- comments
- whitespace
- top-level declarations

A file is encoded as UTF-8.

A file MAY import other AzmaIDL files.

The implementation MUST treat the imported document set as a single semantic program after parsing and import resolution.

---

# 4. Lexical Structure

## 4.1 Character Set

AzmaIDL source files are UTF-8 encoded text.

Identifiers and keywords are ASCII-only in v0.1.

String contents MAY contain arbitrary UTF-8.

## 4.2 Whitespace

Whitespace consists of:
- space `U+0020`
- tab `U+0009`
- carriage return `U+000D`
- line feed `U+000A`

Whitespace MAY appear between tokens unless otherwise specified.

## 4.3 Line Comments

A line comment begins with `--` and continues until end of line.

Example:
```azmaidl
-- this is a comment
```

## 4.4 Block Comments / Doc Comments

A block comment begins with a line containing exactly `---` and ends with a later line containing exactly `---`.

Everything between is comment text.

Example:
```azmaidl
---
This is a block comment.
It may document the next declaration.
---
```

A parser MAY attach the most recent preceding block comment to the next declaration as documentation metadata.

Nested block comments are not supported.

## 4.5 Identifiers

An identifier MUST match:

```text
[A-Za-z_][A-Za-z0-9_\-\.]*
```

Examples:
- `metadata`
- `section`
- `input-data`
- `builtin.no-crash`
- `football_api`

Identifiers are case-sensitive in the grammar, but declaration keywords are canonically lowercase.

Implementations SHOULD reject mixed-case keywords like `%Metadata` unless they intentionally support compatibility mode.

## 4.6 Keywords

Reserved directive keywords in v0.1:

- `metadata`
- `import`
- `config`
- `api`
- `symbol`
- `section`
- `fixture`
- `case-set`
- `stencil`
- `shuffler`
- `input-data`
- `oracle`
- `harness`
- `seed-corpus`
- `dictionary`
- `emit`
- `requires`
- `tag`

Reserved section names:
- `unit`
- `fuzz`
- `shared`

## 4.7 Literals

### 4.7.1 String Literal
A string literal is enclosed in double quotes.

Escape sequences supported in v0.1:
- `\"`
- `\\`
- `\n`
- `\r`
- `\t`

Example:
```azmaidl
"name"
```

### 4.7.2 Multiline String Literal
A multiline string literal is enclosed in `{{` and `}}`.

Example:
```azmaidl
summary = {{
This is a multiline string.
}}
```

Rules:
- the content is everything between the delimiters
- the delimiters are not included in the value
- implementations SHOULD preserve line endings as normalized `\n`
- no escape processing is required inside multiline strings

### 4.7.3 Integer Literal
An integer literal is a base-10 signed or unsigned integer.

Examples:
- `0`
- `42`
- `-1`

### 4.7.4 Boolean Literal
Boolean literals:
- `true`
- `false`

### 4.7.5 Range Literal
A range literal is:
```text
<integer> .. <integer>
```

Example:
```azmaidl
-1..1000
```

The lower bound MAY be greater than the upper bound syntactically, but MUST be rejected semantically unless a construct explicitly permits descending ranges. v0.1 does not.

---

# 5. Tokens and Delimiters

The following delimiters and punctuators are part of the language:

- `%`
- `{`
- `}`
- `[[`
- `]]`
- `(`
- `)`
- `[`
- `]`
- `,`
- `=`
- `;`

The range operator:
- `..`

The interpolation marker syntax is reserved for strings, see §10.8.

---

# 6. High-Level Syntax

AzmaIDL supports two declaration forms:

## 6.1 Property Block Declaration

```azmaidl
%keyword "optional-name" [[
    key = value
    key = value
]];
```

## 6.2 Nested Block Declaration

```azmaidl
%keyword "optional-name" {
    declarations
}
```

A declaration MAY also combine a name with a nested block depending on the keyword.

Every property block declaration MUST terminate with `;`.

Nested block declarations do not require `;`.

---

# 7. Grammar Sketch

This is a normative grammar sketch, not a full parser grammar.

```text
file                := { top_decl | comment | whitespace }

top_decl            := property_decl | nested_decl

property_decl       := "%" ident opt_name property_block ";"
nested_decl         := "%" ident opt_name nested_block

opt_name            := string_lit | ε

property_block      := "[[" { property } "]]"
property            := ident "=" value

nested_block        := "{" { top_decl | comment | whitespace } "}"

value               := string_lit
                    | multiline_string
                    | integer
                    | boolean
                    | range
                    | list
                    | record
                    | call_expr
                    | identifier_ref

list                := "[" [ value { "," value } ] "]"

record              := "{" [ record_field { "," record_field } ] "}"
record_field        := ident "=" value

call_expr           := ident "(" [ arg_list ] ")"
                    | ident "!" "(" [ arg_list ] ")"

arg_list            := value { "," value }

identifier_ref      := ident
```

Notes:
- Within `[[ ... ]]`, properties are separated by line breaks, not commas.
- A property key MUST NOT appear more than once within the same property block unless that property is explicitly defined as repeatable. v0.1 defines no repeatable scalar properties.
- A parser MAY allow trailing commas in lists and records as an extension, but strict v0.1 does not require this.

---

# 8. Data Model

All parsed values normalize into one of these semantic types:

- `string`
- `multiline-string` (normalizes to string)
- `integer`
- `boolean`
- `range`
- `list`
- `record`
- `call`
- `identifier-ref`

The semantic validator resolves:
- symbol references
- API references
- section references
- builtins
- target expressions

---

# 9. Namespaces and Scoping

## 9.1 Global Scope

The following declarations create names in global scope:
- `%api`
- `%section`
- `%fixture`
- `%case-set`
- `%stencil`
- `%shuffler`
- `%input-data`
- `%oracle`
- `%harness`
- `%dictionary`

`%metadata`, `%config`, `%emit`, `%requires`, `%tag`, `%import` do not create named global objects unless named explicitly by future versions.

## 9.2 Nested Scope

Inside `%api`, `%section`, or `%shared` blocks, nested declarations are scoped to the containing declaration but are also addressable by fully qualified names.

Example:
- global `Football`
- nested symbol `Shoot`
- fully qualified symbol name: `Football::Shoot`

An implementation MAY also provide a canonical internal form such as `Football/Shoot`.

## 9.3 Name Resolution

Unqualified references resolve in this order:
1. local nested scope
2. containing section or API scope
3. global scope
4. builtin namespace

If a name is ambiguous, the implementation MUST reject it with a diagnostic.

---

# 10. Core Value Forms

## 10.1 Strings
Plain strings represent textual values.

## 10.2 Integers
Integers represent counts, seeds, limits, timeouts, and numeric constraints.

## 10.3 Booleans
Booleans represent switches and mode flags.

## 10.4 Ranges
Ranges represent constrained integer domains and sizes.

## 10.5 Lists
Lists represent ordered collections.

## 10.6 Records
Records represent named structured values.

## 10.7 Call Expressions
Call expressions provide structured references and builtin helpers.

The following call forms are reserved in v0.1:

- `glob!(string)`
- `symbol(string)`
- `symbols(record-like filters...)`

Additional builtin call forms MAY be implemented as extensions, but only the above are normative.

### `glob!(pattern)`
Represents a file glob pattern.
Argument count: exactly 1 string.

### `symbol(name)`
Resolves to a single symbol reference.
Argument count: exactly 1 string.

### `symbols(...)`
Represents a symbol selector query.
The arguments are implementation-defined in AST form but MUST support named constraints through record-like syntax in property values. Recommended usage is:
```azmaidl
symbols(kind = "function")
```
Since the core grammar supports positional call arguments only, implementations SHOULD parse `symbols(kind = "function")` as a special builtin selector expression.

## 10.8 String Interpolation
String interpolation is permitted in selected properties that explicitly allow templating.

Interpolation syntax:
```text
${name}
```

Supported placeholders in v0.1:
- `${symbol}`
- `${api}`
- `${section}`
- `${index}`
- `${namespace}`

Unknown placeholders MUST be rejected.

Interpolation is only semantic in designated properties such as naming patterns. Elsewhere it is treated as raw text.

---

# 11. Top-Level Declarations

## 11.1 `%metadata`

Declares suite-wide metadata.

### Syntax
```azmaidl
%metadata [[
    key = value
]];
```

### Allowed properties
- `name`: string, REQUIRED
- `author`: string, OPTIONAL
- `page`: string, OPTIONAL
- `summary`: string, OPTIONAL
- `license`: string, OPTIONAL
- `version`: string, OPTIONAL
- `namespace`: identifier-like string, OPTIONAL
- `seed`: integer, OPTIONAL
- `language`: string, OPTIONAL
- `dialect`: string, OPTIONAL

### Constraints
- At most one `%metadata` declaration SHOULD appear across the full program.
- If more than one appears, the implementation MUST either reject or define deterministic merge semantics. v0.1 requires rejection.

### Semantics
Defines suite-level defaults and descriptive metadata.

---

## 11.2 `%import`

Imports another AzmaIDL file.

### Syntax
```azmaidl
%import "path/to/file.azmaidl";
```

Canonical property form:
```azmaidl
%import [[
    path = "path/to/file.azmaidl"
]];
```

However, for convenience v0.1 permits the shorthand string-name form:
```azmaidl
%import "path/to/file.azmaidl" [[ ]];
```
Implementations MAY instead support `%import "..." ;` as a dedicated extension. If you want strictness, keep only the property form.

### Allowed properties
- `path`: string, REQUIRED

### Constraints
- Import cycles MUST be detected and rejected unless an implementation explicitly supports cycle-safe once-only resolution.
- Duplicate imports SHOULD be ignored after first successful resolution.

---

## 11.3 `%config`

Declares default configuration.

### Syntax
```azmaidl
%config [[
    key = value
]];
```

May appear globally or inside `%section`.

### Global allowed properties
- `output-dir`: string
- `emit`: list of strings
- `namespace`: string
- `seed`: integer
- `deterministic`: boolean
- `language`: string
- `unit-header`: string
- `fuzz-header`: string

### Section allowed properties
Depends on section kind; see sections below.

### Semantics
Provides defaults inherited by nested declarations in the same scope.

---

## 11.4 `%api`

Declares an API surface.

### Forms

#### Property form
```azmaidl
%api "Football" [[
    interface = "The Football API"
    desc      = "Description"
    files     = glob!("include/football/*.h")
    language  = "c"
]];
```

#### Nested form
```azmaidl
%api "Football" {
    ...
}
```

Both forms MAY be combined by allowing a property form followed by a nested form with the same name; implementations SHOULD merge them if deterministic. Simpler v0.1 rule: one `%api` declaration per API name, but it MAY contain nested declarations.

### Name
REQUIRED.

### Allowed properties
- `interface`: string, OPTIONAL
- `desc`: string, OPTIONAL
- `files`: call expression `glob!(...)` or list of strings/globs, OPTIONAL
- `language`: string, OPTIONAL, values SHOULD be `"c"` or `"c++"`
- `headers`: list of strings, OPTIONAL
- `include-prefix`: string, OPTIONAL
- `namespace`: string, OPTIONAL

### Allowed nested declarations
- `%symbol`
- `%requires`
- `%tag`

### Constraints
- API names MUST be unique globally.

---

## 11.5 `%symbol`

Declares a symbol inside an API.

### Syntax
```azmaidl
%symbol "Shoot" [[
    kind    = "function"
    returns = "void"
    formals = [
        { name = "foo", type = "int" },
        { name = "bar", type = "string" }
    ]
]];
```

### Context
MUST appear inside `%api`.

### Name
REQUIRED.

### Allowed properties
- `kind`: string, REQUIRED
- `returns`: string, OPTIONAL
- `formals`: list of records, OPTIONAL
- `desc`: string, OPTIONAL
- `linkage`: string, OPTIONAL
- `variadic`: boolean, OPTIONAL
- `const`: boolean, OPTIONAL
- `volatile`: boolean, OPTIONAL
- `tags`: list of strings, OPTIONAL

### Allowed `kind` values
- `"function"`
- `"struct"`
- `"enum"`
- `"typedef"`
- `"macro"`
- `"variable"`

### Formal record schema
Each formal record MAY contain:
- `name`: string, REQUIRED
- `type`: string, REQUIRED
- `dir`: string, OPTIONAL (`"in"`, `"out"`, `"inout"`)
- `nullable`: boolean, OPTIONAL
- `size`: range or integer, OPTIONAL
- `desc`: string, OPTIONAL

### Constraints
- For `kind = "function"`, `returns` SHOULD be present.
- For non-function kinds, `returns` MUST NOT be present.
- `formals` MUST only appear for `kind = "function"`.

---

# 12. Section Declarations

## 12.1 `%section`

Declares a generation section.

### Syntax
```azmaidl
%section unit "FootballUnits" {
    ...
}
```

or
```azmaidl
%section fuzz "FootballFuzz" {
    ...
}
```

### Name
Optional but strongly recommended.

### Allowed section kinds
- `unit`
- `fuzz`
- `shared`

### Section-specific nested declarations
- `unit`: `%config`, `%fixture`, `%case-set`, `%stencil`, `%shuffler`, `%requires`, `%tag`
- `fuzz`: `%config`, `%input-data`, `%oracle`, `%harness`, `%seed-corpus`, `%dictionary`, `%requires`, `%tag`
- `shared`: `%config`, `%fixture`, `%case-set`, `%input-data`, `%dictionary`, `%requires`, `%tag`

### Constraints
- Section names MUST be unique per section kind unless an implementation defines merge semantics. v0.1 requires uniqueness.

---

# 13. Unit Section Declarations

## 13.1 `%fixture`

Declares setup/teardown fixture info.

### Syntax
```azmaidl
%fixture "FootballFixture" [[
    setup    = "football_fixture_init"
    teardown = "football_fixture_destroy"
    storage  = "FootballFixture"
]];
```

### Allowed properties
- `setup`: string, OPTIONAL
- `teardown`: string, OPTIONAL
- `storage`: string, OPTIONAL
- `scope`: string, OPTIONAL (`"test"`, `"suite"`)
- `desc`: string, OPTIONAL

### Constraints
- At least one of `setup`, `teardown`, `storage` SHOULD be present.

### Semantics
Pure metadata for generated code; it does not define code bodies.

---

## 13.2 `%case-set`

Declares reusable deterministic unit input cases.

### Syntax
```azmaidl
%case-set "boundary-values" [[
    values = [
        { foo = -1, bar = "" },
        { foo = 0, bar = "a" }
    ]
]];
```

### Allowed properties
- `values`: list, REQUIRED
- `desc`: string, OPTIONAL
- `tags`: list of strings, OPTIONAL

### Constraints
- `values` MUST be a list.
- Elements SHOULD be records when targeting function parameterized tests.

---

## 13.3 `%stencil`

Declares a unit test generation template.

### Syntax
```azmaidl
%stencil "function-smoke" [[
    applies_to = symbols(kind = "function")
    params     = "default-values"
    asserts    = ["does-not-crash"]
    naming     = "test_${symbol}_smoke"
]];
```

### Allowed properties
- `applies_to`: selector expression, REQUIRED
- `instantiate`: string, OPTIONAL
- `params`: string or record or list, OPTIONAL
- `case-set`: string, OPTIONAL
- `fixture`: string, OPTIONAL
- `asserts`: list of strings, OPTIONAL
- `naming`: string, OPTIONAL
- `desc`: string, OPTIONAL
- `tags`: list of strings, OPTIONAL
- `enabled`: boolean, OPTIONAL

### Allowed `instantiate` values
- `"per-symbol"` (default)
- `"per-case"`
- `"per-symbol-per-case"`

### Allowed builtin `params` values
- `"default-values"`
- `"zero-values"`
- `"boundary-values"`
- `"empty-values"`
- `"null-when-possible"`

### Allowed builtin `asserts` values
- `"does-not-crash"`
- `"returns-valid"`
- `"returns-nonnull"`
- `"returns-zero-or-positive"`
- `"errno-unchanged"`

Implementations MAY support more.

### Constraints
- `applies_to` MUST resolve to a symbol selector.
- If `case-set` is present, it MUST resolve to an existing `%case-set`.
- If `fixture` is present, it MUST resolve to an existing `%fixture`.

---

## 13.4 `%shuffler`

Declares deterministic or configured ordering behavior.

### Syntax
```azmaidl
%shuffler "seeded-basic" [[
    mode = "deterministic"
    seed = 42
]];
```

### Allowed properties
- `mode`: string, REQUIRED
- `seed`: integer, OPTIONAL

### Allowed `mode` values
- `"none"`
- `"deterministic"`
- `"per-run"`
- `"per-build"`

### Constraints
- If `mode = "deterministic"`, `seed` SHOULD be present.
- A generator MAY still normalize `per-run` into a reproducible runtime parameter.

---

# 14. Fuzz Section Declarations

## 14.1 `%input-data`

Declares a fuzz input schema.

### Syntax
```azmaidl
%input-data "shoot-input" [[
    kind = "record"
    fields = [
        { name = "foo", type = "int", range = -1..1000 },
        { name = "bar", type = "string", size = 0..128 }
    ]
]];
```

### Allowed properties
- `kind`: string, REQUIRED
- `fields`: list, OPTIONAL
- `type`: string, OPTIONAL
- `range`: range, OPTIONAL
- `size`: range, OPTIONAL
- `charset`: string, OPTIONAL
- `nullable`: boolean, OPTIONAL
- `elements`: record or string, OPTIONAL
- `distribution`: string, OPTIONAL
- `desc`: string, OPTIONAL
- `tags`: list of strings, OPTIONAL

### Allowed `kind` values
- `"int"`
- `"u32"`
- `"u64"`
- `"string"`
- `"bytes"`
- `"array"`
- `"record"`
- `"enum"`
- `"bool"`

### Constraints by kind
- `record` MUST provide `fields`
- scalar kinds MAY provide `range`
- `string` MAY provide `size` and `charset`
- `array` SHOULD provide `elements` and `size`
- `enum` SHOULD provide allowed values through `fields` or an extension property such as `variants`

### Field record schema
- `name`: string, REQUIRED
- `type`: string, REQUIRED unless nested schema used
- `range`: range, OPTIONAL
- `size`: range, OPTIONAL
- `charset`: string, OPTIONAL
- `nullable`: boolean, OPTIONAL
- `desc`: string, OPTIONAL

---

## 14.2 `%oracle`

Declares a fuzz oracle.

### Syntax
```azmaidl
%oracle "no-crash" [[
    kind = "builtin.no-crash"
]];
```

### Allowed properties
- `kind`: string, REQUIRED
- `callback`: string, OPTIONAL
- `desc`: string, OPTIONAL

### Allowed builtin kinds
- `"builtin.no-crash"`
- `"builtin.non-null-return"`
- `"builtin.valid-enum"`
- `"builtin.zero-is-success"`
- `"builtin.no-leak"` (advisory only; requires runtime/tool support)

### Constraints
- If `kind` begins with `builtin.`, `callback` MUST NOT be present.
- If `kind = "custom"`, `callback` MUST be present.

---

## 14.3 `%harness`

Declares a fuzz harness binding.

### Syntax
```azmaidl
%harness "shoot-harness" [[
    target     = symbol("Shoot")
    input      = "shoot-input"
    oracle     = "no-crash"
    timeout_ms = 50
]];
```

### Allowed properties
- `target`: symbol reference, REQUIRED
- `input`: string, REQUIRED
- `oracle`: string, OPTIONAL
- `iterations`: integer, OPTIONAL
- `timeout_ms`: integer, OPTIONAL
- `max_len`: integer, OPTIONAL
- `mutator`: string, OPTIONAL
- `dictionary`: string, OPTIONAL
- `seed`: integer, OPTIONAL
- `enabled`: boolean, OPTIONAL
- `desc`: string, OPTIONAL
- `tags`: list of strings, OPTIONAL

### Constraints
- `target` MUST resolve to a function symbol.
- `input` MUST resolve to an existing `%input-data`.
- `oracle`, if present, MUST resolve to an existing `%oracle`.
- `dictionary`, if present, MUST resolve to an existing `%dictionary`.

---

## 14.4 `%seed-corpus`

Declares seed input values.

### Syntax
```azmaidl
%seed-corpus "shoot-seeds" [[
    values = [
        { foo = 0, bar = "" },
        { foo = 1, bar = "goal" }
    ]
]];
```

### Name
OPTIONAL.

### Allowed properties
- `values`: list, REQUIRED
- `input`: string, OPTIONAL
- `desc`: string, OPTIONAL

### Constraints
- If nested in a fuzz section and unqualified, a `%seed-corpus` SHOULD be associated with harnesses by matching `input`.
- If `input` is specified, it MUST resolve to `%input-data`.

---

## 14.5 `%dictionary`

Declares dictionary entries for mutation and generation assistance.

### Syntax
```azmaidl
%dictionary "football-terms" [[
    entries = ["goal", "red", "penalty", "offside"]
]];
```

### Allowed properties
- `entries`: list of strings, REQUIRED
- `desc`: string, OPTIONAL

### Constraints
- All entries MUST be strings.

---

# 15. Cross-Cutting Declarations

## 15.1 `%emit`

Declares explicit generation outputs.

### Syntax
```azmaidl
%emit [[
    target = "combined"
    path   = "generated/tests.c"
]];
```

### Allowed properties
- `target`: string, REQUIRED
- `path`: string, REQUIRED
- `section`: string, OPTIONAL

### Allowed `target` values
- `"unit-c"`
- `"fuzz-c"`
- `"combined"`
- `"manifest"`

### Constraints
- `section`, if present, MUST resolve to an existing `%section`.

---

## 15.2 `%requires`

Declares generation prerequisites.

### Syntax
```azmaidl
%requires [[
    headers = ["football.h"]
    symbols = ["Shoot"]
]];
```

### Allowed properties
- `headers`: list of strings, OPTIONAL
- `symbols`: list of strings, OPTIONAL
- `apis`: list of strings, OPTIONAL
- `platforms`: list of strings, OPTIONAL

### Semantics
If requirements are unmet, the implementation MAY skip generation or emit diagnostics depending on mode.

---

## 15.3 `%tag`

Declares tags on the enclosing declaration or block.

### Syntax
```azmaidl
%tag [[
    values = ["slow", "api", "fuzz"]
]];
```

### Allowed properties
- `values`: list of strings, REQUIRED

### Context
May appear nested under `%api`, `%symbol`, `%section`, `%stencil`, `%harness`, etc.

### Semantics
Tags are inherited by generated instances unless overridden.

---

# 16. Selector Semantics

## 16.1 `symbol("Name")`
Resolves exactly one symbol by unqualified or fully qualified name.

Errors:
- zero matches
- multiple matches

## 16.2 `symbols(...)`
Selects a set of symbols.

Supported filter keys in v0.1:
- `kind`
- `returns`
- `name`
- `tag`
- `api`

Examples:
```azmaidl
symbols(kind = "function")
symbols(kind = "function", returns = "void")
symbols(api = "Football", kind = "function")
```

### Semantics
A selector returns an ordered set of symbols.
Default order is declaration order unless a shuffler or expansion rule changes it.

---

# 17. Inheritance and Defaulting

## 17.1 Config Inheritance
Nested declarations inherit `%config` values from enclosing scope unless overridden.

Example:
- global `%config seed = 1337`
- section `%config seed = 42`
- harness without `seed` uses `42`

## 17.2 Metadata Visibility
`%metadata` is globally visible to all emitters.

## 17.3 Tag Inheritance
Tags on enclosing declarations SHOULD be inherited by nested declarations and generated instances.

---

# 18. Validation Rules

Implementations MUST validate at least:

## 18.1 Structural Validation
- correct declaration nesting
- legal property names
- legal property value types
- required properties present
- no duplicate singleton properties

## 18.2 Reference Validation
- referenced APIs exist
- referenced symbols exist
- referenced fixtures/case-sets/oracles/input-data/dictionaries exist

## 18.3 Semantic Validation
- `%symbol` kinds and properties agree
- harness targets are functions
- ranges are ascending
- lists contain valid element types
- seeds, timeouts, iterations are non-negative unless explicitly allowed

## 18.4 Determinism Validation
If deterministic generation mode is enabled:
- all shufflers MUST have known seeds or fixed order
- all expansions MUST be reproducible

---

# 19. Expansion Semantics

AzmaIDL declarations may expand into multiple generated test instances.

## 19.1 Stencil Expansion
A `%stencil` expands over its selected symbols.

Given:
- selector result `S`
- optional case-set `C`
- instantiate mode `M`

Then:
- `per-symbol` → one generated test per symbol
- `per-case` → one generated test per case, if symbol context is singular or external
- `per-symbol-per-case` → cartesian product of symbols and cases

Generated names MUST be stable and deterministic.

## 19.2 Harness Expansion
Each `%harness` creates one fuzz registration unit unless a future version allows multiplicity.

## 19.3 Seed Corpus Association
A `%seed-corpus` MAY be associated with:
- all harnesses using matching `input`
- a named harness by future extension
- a section-wide default if unambiguous

---

# 20. Emission Model

An implementation processes AzmaIDL in the following phases:

1. parse
2. resolve imports
3. validate syntax and schema
4. build IR
5. resolve references
6. inherit defaults
7. expand templates/selectors
8. emit outputs

Outputs MAY include:
- unit test C source
- fuzz harness C source
- combined source
- manifest metadata

The emitted code targets `AzmaUnit.h` and `AzmaFuzz.h`, but the exact generated API surface is implementation-defined.

---

# 21. Error Handling

A conforming implementation MUST produce diagnostics containing:
- file path
- line and column if available
- error class
- message

Recommended error classes:
- lexical error
- syntax error
- schema error
- resolution error
- type error
- semantic error

Implementations SHOULD continue after recoverable errors to report more than one issue.

---

# 22. Canonical Formatting Recommendations

These are non-normative but recommended:

- keywords lowercase
- 4-space indentation inside blocks
- one property per line
- lists and records compact but readable
- strings for symbolic names
- semicolon after every `[[...]]` declaration

Example:
```azmaidl
%stencil "function-smoke" [[
    applies_to = symbols(kind = "function")
    params     = "default-values"
    asserts    = ["does-not-crash"]
    naming     = "test_${symbol}_smoke"
]];
```

---

# 23. Full Example

```azmaidl
%metadata [[
    name      = "MyProgram"
    author    = "John Mie Self"
    page      = "https://github.com/mieslef/MyProgram"
    summary   = {{
        MyProgram rocks.
        So you better fuzz it.
    }}
    namespace = "myprogram"
    seed      = 1337
]];

%config [[
    output-dir    = "generated"
    deterministic = true
    unit-header   = "AzmaUnit.h"
    fuzz-header   = "AzmaFuzz.h"
]];

%api "Football" {
    %tag [[
        values = ["sports", "public-api"]
    ]];

    %symbol "Shoot" [[
        kind    = "function"
        returns = "void"
        formals = [
            { name = "foo", type = "int" },
            { name = "bar", type = "string" }
        ]
    ]];

    %symbol "Score" [[
        kind    = "function"
        returns = "int"
        formals = [
            { name = "team", type = "string" }
        ]
    ]];
}

%section unit "FootballUnits" {
    %config [[
        api      = "Football"
        seed     = 42
        fixture  = "FootballFixture"
    ]];

    %fixture "FootballFixture" [[
        setup    = "football_fixture_init"
        teardown = "football_fixture_destroy"
        storage  = "FootballFixture"
    ]];

    %case-set "basic-cases" [[
        values = [
            { foo = 0, bar = "" },
            { foo = 1, bar = "goal" }
        ]
    ]];

    %stencil "smoke" [[
        applies_to  = symbols(api = "Football", kind = "function")
        instantiate = "per-symbol"
        params      = "default-values"
        asserts     = ["does-not-crash"]
        naming      = "test_${symbol}_smoke"
        fixture     = "FootballFixture"
    ]];

    %stencil "boundary" [[
        applies_to  = symbols(api = "Football", kind = "function")
        instantiate = "per-symbol-per-case"
        case-set    = "basic-cases"
        asserts     = ["does-not-crash"]
        naming      = "test_${symbol}_case_${index}"
    ]];

    %shuffler "seeded-basic" [[
        mode = "deterministic"
        seed = 42
    ]];
}

%section fuzz "FootballFuzz" {
    %config [[
        api        = "Football"
        seed       = 101
        iterations = 50000
    ]];

    %input-data "shoot-input" [[
        kind = "record"
        fields = [
            { name = "foo", type = "int", range = -1..1000 },
            { name = "bar", type = "string", size = 0..128 }
        ]
    ]];

    %oracle "no-crash" [[
        kind = "builtin.no-crash"
    ]];

    %dictionary "football-terms" [[
        entries = ["goal", "red", "penalty", "offside"]
    ]];

    %harness "shoot-harness" [[
        target      = symbol("Shoot")
        input       = "shoot-input"
        oracle      = "no-crash"
        dictionary  = "football-terms"
        timeout_ms  = 50
        iterations  = 10000
    ]];

    %seed-corpus "shoot-seeds" [[
        input = "shoot-input"
        values = [
            { foo = 0, bar = "" },
            { foo = 1, bar = "goal" },
            { foo = 999, bar = "offside" }
        ]
    ]];
}

%emit [[
    target = "combined"
    path   = "generated/football_tests.c"
]];
```

---

# 24. Minimal Implementation Requirements

An AzmaIDL v0.1 implementation claiming full support MUST:

- parse UTF-8 source
- support comments and block comments
- parse all declaration forms in this spec
- support all normative builtin value types
- support `%metadata`, `%api`, `%symbol`, `%section`, `%stencil`, `%input-data`, `%harness`
- resolve `symbol(...)` and `symbols(...)`
- validate required properties and references
- emit deterministic IR suitable for source generation

---

# 25. Recommended Future Extensions

Non-normative, not part of v0.1:
- explicit enum variant schemas
- unions and tagged variants
- custom mutators
- constrained string regexes
- binary layout schemas
- linker/autoregistration metadata
- coverage-guided engine adapters
- shrinkers for property testing
- conditional generation
- user-defined named selectors
- safe inline C fragments in fenced blocks

---

# 26. Final Design Notes

The most important discipline for AzmaIDL is:

- keep it declarative
- keep generation deterministic
- let the runtime headers stay small
- avoid turning `[[...]]` into a scripting language
- prefer schema validation over raw code embedding

