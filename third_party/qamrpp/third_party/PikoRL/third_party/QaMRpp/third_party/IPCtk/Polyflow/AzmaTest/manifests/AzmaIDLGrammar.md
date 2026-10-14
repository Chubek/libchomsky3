# AzmaIDL v0.1 EBNF

## 1. Conventions

This grammar uses a conventional EBNF style:

- `::=` means “is defined as”
- `|` means alternation
- `{ ... }` means repetition, zero or more
- `[ ... ]` means optional
- parentheses group productions
- terminals are shown in quotes

This grammar is split into:
1. **lexical grammar**
2. **syntactic grammar**
3. **notes for context-sensitive validation**

Some constraints from the spec are semantic and cannot be expressed cleanly in pure EBNF, so those are listed afterward.

---

# 2. Lexical Grammar

## 2.1 Source File

```ebnf
source-file ::= { trivia | top-level-decl } ;
```

---

## 2.2 Trivia

```ebnf
trivia ::= whitespace | line-comment | block-comment ;
```

```ebnf
whitespace ::= " " | "\t" | "\r" | "\n" ;
```

```ebnf
line-comment ::= "--" { line-comment-char } newline ;
```

```ebnf
line-comment-char ::= ? any character except "\r" and "\n" ? ;
```

```ebnf
newline ::= "\n" | "\r\n" | "\r" ;
```

For block comments, the original spec used `--- ... ---`. Lexically:

```ebnf
block-comment ::= "---" { block-comment-char } "---" ;
```

```ebnf
block-comment-char ::= ? any character sequence not containing the terminating "---" ? ;
```

Implementation note: this is easiest treated as a lexer rule, not a parser rule.

---

## 2.3 Identifiers

```ebnf
identifier ::= ident-start { ident-continue } ;
```

```ebnf
ident-start ::= letter | "_" ;
ident-continue ::= letter | digit | "_" | "-" ;
```

```ebnf
letter ::= "A" | "B" | "C" | "D" | "E" | "F" | "G"
         | "H" | "I" | "J" | "K" | "L" | "M" | "N"
         | "O" | "P" | "Q" | "R" | "S" | "T" | "U"
         | "V" | "W" | "X" | "Y" | "Z"
         | "a" | "b" | "c" | "d" | "e" | "f" | "g"
         | "h" | "i" | "j" | "k" | "l" | "m" | "n"
         | "o" | "p" | "q" | "r" | "s" | "t" | "u"
         | "v" | "w" | "x" | "y" | "z" ;
```

```ebnf
digit ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
```

---

## 2.4 Keywords

The following are reserved in declaration position:

```ebnf
keyword ::= "metadata"
          | "import"
          | "config"
          | "api"
          | "symbol"
          | "section"
          | "fixture"
          | "case-set"
          | "stencil"
          | "shuffler"
          | "input-data"
          | "oracle"
          | "harness"
          | "seed-corpus"
          | "dictionary"
          | "emit"
          | "requires"
          | "tag" ;
```

The following are treated as reserved literals/atoms in value position:

```ebnf
reserved-atom ::= "true" | "false" | "unit" | "fuzz" | "shared" ;
```

You may also choose to reserve builtin oracle kinds such as:

```ebnf
oracle-kind ::= "returns"
              | "not_null"
              | "null"
              | "zero"
              | "nonzero"
              | "eq"
              | "memory_eq"
              | "crash_free" ;
```

These can also be treated as ordinary strings semantically, if you prefer.

---

## 2.5 Literals

## Integer

```ebnf
integer ::= ["-"] digit { digit } ;
```

---

## Boolean

```ebnf
boolean ::= "true" | "false" ;
```

---

## String

```ebnf
string ::= "\"" { string-char } "\"" ;
```

```ebnf
string-char ::= unescaped-char | escape-seq | interpolation ;
```

```ebnf
unescaped-char ::= ? any character except "\"", "\" and line terminators ? ;
```

```ebnf
escape-seq ::= "\\\"" | "\\\\" | "\\n" | "\\r" | "\\t" | "\\{" | "\\}" ;
```

Interpolation is lexical/syntactic hybrid:

```ebnf
interpolation ::= "${" interpolation-expr "}" ;
```

For v0.1, keep interpolation expression narrow:

```ebnf
interpolation-expr ::= identifier { "." identifier } ;
```

If you want the parser simpler, you can tokenize string as opaque and parse interpolation later.

---

## Multiline String

```ebnf
multiline-string ::= "{{" multiline-body "}}" ;
```

```ebnf
multiline-body ::= ? any character sequence not containing the terminating "}}" ? ;
```

Again, easiest as a lexer rule.

---

## Range

```ebnf
range ::= integer ".." integer ;
```

---

# 3. Syntactic Grammar

## 3.1 Top-Level Structure

```ebnf
top-level-decl ::= property-block-decl
                 | nested-block-decl ;
```

```ebnf
property-block-decl ::= "%" property-keyword "[[" property-list "]]" ";" ;
```

```ebnf
nested-block-decl ::= "%" nested-keyword [ identifier ] "{" { nested-member } "}" ;
```

Not every keyword can appear in both forms, so concrete productions below narrow this.

---

## 3.2 Top-Level Declarations

```ebnf
top-level-decl ::= metadata-decl
                 | import-decl
                 | config-decl
                 | api-decl
                 | section-decl
                 | emit-decl
                 | requires-decl
                 | tag-decl ;
```

---

## 3.3 Metadata

```ebnf
metadata-decl ::= "%" "metadata" "[[" property-list "]]" ";" ;
```

Example:

```txt
%metadata [[
    package = "azma.examples";
    version = "0.1";
]];
```

---

## 3.4 Import

```ebnf
import-decl ::= "%" "import" "[[" import-property-list "]]" ";" ;
```

```ebnf
import-property-list ::= import-property { property-sep import-property } [ property-sep ] ;
```

```ebnf
import-property ::= "path" "=" string
                  | "as" "=" identifier
                  | "mode" "=" string ;
```

---

## 3.5 Config

```ebnf
config-decl ::= "%" "config" [ identifier ] "[[" property-list "]]" ";" ;
```

If you want named configs only, remove the optional identifier.

---

## 3.6 API

```ebnf
api-decl ::= "%" "api" identifier "{" { api-member } "}" ;
```

```ebnf
api-member ::= symbol-decl
             | requires-decl
             | tag-decl
             | config-decl ;
```

---

## 3.7 Symbol

```ebnf
symbol-decl ::= "%" "symbol" identifier "[[" property-list "]]" ";" ;
```

Typical properties are semantic rather than grammatical, e.g. `link_name`, `signature`, `returns`, `params`, `header`, `tags`.

---

## 3.8 Section

```ebnf
section-decl ::= "%" "section" section-kind identifier "{" { section-member } "}" ;
```

```ebnf
section-kind ::= "unit" | "fuzz" | "shared" ;
```

```ebnf
section-member ::= fixture-decl
                 | case-set-decl
                 | stencil-decl
                 | shuffler-decl
                 | input-data-decl
                 | oracle-decl
                 | harness-decl
                 | seed-corpus-decl
                 | dictionary-decl
                 | config-decl
                 | emit-decl
                 | requires-decl
                 | tag-decl ;
```

Context-sensitive validation must restrict which member kinds are legal in `unit`, `fuzz`, or `shared` sections.

---

# 4. Unit Section Grammar

## 4.1 Fixture

```ebnf
fixture-decl ::= "%" "fixture" identifier "[[" property-list "]]" ";" ;
```

---

## 4.2 Case Set

```ebnf
case-set-decl ::= "%" "case-set" identifier "[[" property-list "]]" ";" ;
```

---

## 4.3 Stencil

```ebnf
stencil-decl ::= "%" "stencil" identifier "{" { stencil-member } "}" ;
```

```ebnf
stencil-member ::= property-statement
                 | requires-decl
                 | tag-decl ;
```

You can also make stencil a property block if you want a simpler parser:

```ebnf
stencil-decl ::= "%" "stencil" identifier "[[" property-list "]]" ";" ;
```

But the nested form is more extensible.

---

## 4.4 Shuffler

```ebnf
shuffler-decl ::= "%" "shuffler" identifier "[[" property-list "]]" ";" ;
```

---

# 5. Fuzz Section Grammar

## 5.1 Input Data

```ebnf
input-data-decl ::= "%" "input-data" identifier "{" { input-data-member } "}" ;
```

```ebnf
input-data-member ::= property-statement
                    | field-decl
                    | requires-decl
                    | tag-decl ;
```

```ebnf
field-decl ::= "%" "field" identifier "[[" property-list "]]" ";" ;
```

If you do not want `%field` as a keyword, define schema entirely through properties. But structurally, `%field` is cleaner.

Since `%field` was not in the earlier keyword set, add it if adopting this form.

---

## 5.2 Oracle

```ebnf
oracle-decl ::= "%" "oracle" identifier "[[" property-list "]]" ";" ;
```

---

## 5.3 Harness

```ebnf
harness-decl ::= "%" "harness" identifier "[[" property-list "]]" ";" ;
```

---

## 5.4 Seed Corpus

```ebnf
seed-corpus-decl ::= "%" "seed-corpus" identifier "[[" property-list "]]" ";" ;
```

---

## 5.5 Dictionary

```ebnf
dictionary-decl ::= "%" "dictionary" identifier "[[" property-list "]]" ";" ;
```

---

# 6. Cross-Cutting Declarations

## 6.1 Emit

```ebnf
emit-decl ::= "%" "emit" "[[" property-list "]]" ";" ;
```

If named emits are useful:

```ebnf
emit-decl ::= "%" "emit" [ identifier ] "[[" property-list "]]" ";" ;
```

---

## 6.2 Requires

```ebnf
requires-decl ::= "%" "requires" "[[" property-list "]]" ";" ;
```

---

## 6.3 Tag

```ebnf
tag-decl ::= "%" "tag" "[[" property-list "]]" ";" ;
```

If you want a simpler tag syntax, you could also support:

```ebnf
tag-decl ::= "%" "tag" identifier ";" ;
```

or both forms.

---

# 7. Property System

This is the core reusable part of the grammar.

## 7.1 Property List

```ebnf
property-list ::= property-statement { property-sep property-statement } [ property-sep ] ;
```

```ebnf
property-sep ::= ";" | "," ;
```

I recommend allowing both separators but canonicalizing to semicolons.

---

## 7.2 Property Statement

```ebnf
property-statement ::= property-key "=" value ;
```

```ebnf
property-key ::= identifier { "." identifier } ;
```

This allows things like:

- `name = "foo"`
- `generator.kind = "bytes"`
- `emit.header.path = "AzmaUnit.h"`

---

# 8. Values

## 8.1 General Value

```ebnf
value ::= scalar
        | range
        | list
        | record
        | call-expr ;
```

```ebnf
scalar ::= string
         | multiline-string
         | integer
         | boolean
         | atom ;
```

```ebnf
atom ::= identifier ;
```

Atoms permit unquoted symbolic values like `unit`, `fuzz`, `returns`, etc.

---

## 8.2 List

```ebnf
list ::= "[" [ value-list ] "]" ;
```

```ebnf
value-list ::= value { "," value } [ "," ] ;
```

---

## 8.3 Record

```ebnf
record ::= "{" [ record-entry-list ] "}" ;
```

```ebnf
record-entry-list ::= record-entry { "," record-entry } [ "," ] ;
```

```ebnf
record-entry ::= record-key "=" value ;
```

```ebnf
record-key ::= identifier | string ;
```

---

## 8.4 Call Expression

```ebnf
call-expr ::= identifier "(" [ argument-list ] ")" ;
```

```ebnf
argument-list ::= argument { "," argument } [ "," ] ;
```

```ebnf
argument ::= value | named-argument ;
```

```ebnf
named-argument ::= identifier "=" value ;
```

This supports:

- `glob!("tests/**/*.bin")`
- `symbol("parse_packet")`
- `symbols(tag="core", returns="int")`

If you want `glob!` with `!` in the function name, define a special production:

```ebnf
call-expr ::= callable-name "(" [ argument-list ] ")" ;
```

```ebnf
callable-name ::= identifier [ "!" ] ;
```

---

# 9. Optional Type Schema Grammar for `%input-data`

If you want `%input-data` to have a richer structural grammar instead of only properties, use this.

## 9.1 Input Type

```ebnf
input-data-member ::= property-statement
                    | schema-decl
                    | requires-decl
                    | tag-decl ;
```

```ebnf
schema-decl ::= "%" "schema" "=" type-expr ";" ;
```

If you want schema as a property, this may be easier:

```ebnf
property-statement ::= property-key "=" value ;
```

with `schema = ...` as data.

## 9.2 Type Expressions

```ebnf
type-expr ::= scalar-type
            | bytes-type
            | string-type
            | array-type
            | record-type
            | enum-type
            | optional-type ;
```

```ebnf
scalar-type ::= "u8" | "u16" | "u32" | "u64"
              | "i8" | "i16" | "i32" | "i64"
              | "bool" ;
```

```ebnf
bytes-type ::= "bytes" "(" [ type-constraint-list ] ")" ;
```

```ebnf
string-type ::= "string" "(" [ type-constraint-list ] ")" ;
```

```ebnf
array-type ::= "array" "(" "of" "=" type-expr [ "," type-constraint-list ] ")" ;
```

```ebnf
record-type ::= "record" "{" { typed-field-decl } "}" ;
```

```ebnf
typed-field-decl ::= identifier ":" type-expr [ field-constraint-block ] ";" ;
```

```ebnf
field-constraint-block ::= "[" type-constraint-list "]" ;
```

```ebnf
enum-type ::= "enum" "(" enum-value-list ")" ;
```

```ebnf
enum-value-list ::= scalar { "," scalar } [ "," ] ;
```

```ebnf
optional-type ::= "optional" "(" type-expr ")" ;
```

```ebnf
type-constraint-list ::= type-constraint { "," type-constraint } [ "," ] ;
```

```ebnf
type-constraint ::= identifier "=" value ;
```

This portion is an optional extension if you want `input-data` to be more than property bags.

---

# 10. Concrete Top-Level Consolidated Grammar

If you want one compact parser-facing version, here it is:

```ebnf
source-file ::= { trivia | top-level-decl } ;

top-level-decl ::= metadata-decl
                 | import-decl
                 | config-decl
                 | api-decl
                 | section-decl
                 | emit-decl
                 | requires-decl
                 | tag-decl ;

metadata-decl ::= "%" "metadata" "[[" property-list "]]" ";" ;
import-decl   ::= "%" "import" "[[" import-property-list "]]" ";" ;
config-decl   ::= "%" "config" [ identifier ] "[[" property-list "]]" ";" ;
api-decl      ::= "%" "api" identifier "{" { api-member } "}" ;
section-decl  ::= "%" "section" section-kind identifier "{" { section-member } "}" ;
emit-decl     ::= "%" "emit" [ identifier ] "[[" property-list "]]" ";" ;
requires-decl ::= "%" "requires" "[[" property-list "]]" ";" ;
tag-decl      ::= "%" "tag" "[[" property-list "]]" ";" ;

api-member ::= symbol-decl
             | config-decl
             | requires-decl
             | tag-decl ;

symbol-decl ::= "%" "symbol" identifier "[[" property-list "]]" ";" ;

section-kind ::= "unit" | "fuzz" | "shared" ;

section-member ::= fixture-decl
                 | case-set-decl
                 | stencil-decl
                 | shuffler-decl
                 | input-data-decl
                 | oracle-decl
                 | harness-decl
                 | seed-corpus-decl
                 | dictionary-decl
                 | config-decl
                 | emit-decl
                 | requires-decl
                 | tag-decl ;

fixture-decl     ::= "%" "fixture" identifier "[[" property-list "]]" ";" ;
case-set-decl    ::= "%" "case-set" identifier "[[" property-list "]]" ";" ;
stencil-decl     ::= "%" "stencil" identifier "{" { stencil-member } "}" ;
shuffler-decl    ::= "%" "shuffler" identifier "[[" property-list "]]" ";" ;
input-data-decl  ::= "%" "input-data" identifier "{" { input-data-member } "}" ;
oracle-decl      ::= "%" "oracle" identifier "[[" property-list "]]" ";" ;
harness-decl     ::= "%" "harness" identifier "[[" property-list "]]" ";" ;
seed-corpus-decl ::= "%" "seed-corpus" identifier "[[" property-list "]]" ";" ;
dictionary-decl  ::= "%" "dictionary" identifier "[[" property-list "]]" ";" ;

stencil-member ::= property-statement
                 | requires-decl
                 | tag-decl ;

input-data-member ::= property-statement
                    | field-decl
                    | requires-decl
                    | tag-decl ;

field-decl ::= "%" "field" identifier "[[" property-list "]]" ";" ;

property-list ::= property-statement { property-sep property-statement } [ property-sep ] ;
property-sep ::= ";" | "," ;
property-statement ::= property-key "=" value ;
property-key ::= identifier { "." identifier } ;

import-property-list ::= import-property { property-sep import-property } [ property-sep ] ;
import-property ::= "path" "=" string
                  | "as" "=" identifier
                  | "mode" "=" string ;

value ::= scalar
        | range
        | list
        | record
        | call-expr ;

scalar ::= string
         | multiline-string
         | integer
         | boolean
         | atom ;

atom ::= identifier ;

range ::= integer ".." integer ;

list ::= "[" [ value-list ] "]" ;
value-list ::= value { "," value } [ "," ] ;

record ::= "{" [ record-entry-list ] "}" ;
record-entry-list ::= record-entry { "," record-entry } [ "," ] ;
record-entry ::= record-key "=" value ;
record-key ::= identifier | string ;

call-expr ::= callable-name "(" [ argument-list ] ")" ;
callable-name ::= identifier [ "!" ] ;
argument-list ::= argument { "," argument } [ "," ] ;
argument ::= value | named-argument ;
named-argument ::= identifier "=" value ;

identifier ::= ident-start { ident-continue } ;
ident-start ::= letter | "_" ;
ident-continue ::= letter | digit | "_" | "-" ;

integer ::= ["-"] digit { digit } ;
boolean ::= "true" | "false" ;

string ::= "\"" { string-char } "\"" ;
string-char ::= unescaped-char | escape-seq | interpolation ;
interpolation ::= "${" interpolation-expr "}" ;
interpolation-expr ::= identifier { "." identifier } ;

multiline-string ::= "{{" multiline-body "}}" ;
```

---

# 11. Context-Sensitive Rules Not Expressible Well in EBNF

These should be enforced in semantic analysis rather than grammar.

## 11.1 Declaration placement
- `%symbol` may only appear inside `%api`.
- `%fixture`, `%case-set`, `%stencil`, `%shuffler` should only appear in `unit` or `shared` sections, depending on your rules.
- `%input-data`, `%oracle`, `%harness`, `%seed-corpus`, `%dictionary` should only appear in `fuzz` or `shared` sections.

## 11.2 Required properties
Examples:
- `%import` must contain `path`.
- `%harness` must contain `target`, `input`, and `oracle`.
- `%oracle` must contain at least `kind`.
- `%symbol` should contain enough signature metadata for codegen.

## 11.3 Reference resolution
- `symbol("name")` must resolve to exactly one `%symbol`.
- `symbols(...)` returns a set and may be empty or non-empty depending on the call site requirements.
- section-local names shadow imported names only within the allowed scope.

## 11.4 Determinism
- No ambiguous ordering in emitted output.
- Expansion of selectors should be stable and deterministic.
- `glob!()` should specify deterministic sorting.

## 11.5 Interpolation constraints
- `${...}` inside strings should only reference visible properties/metadata in scope.
- cyclic interpolation references should be rejected.

---

# 12. Recommended Parser Strategy

For implementation, I’d recommend:

## Lexer
Tokenize:
- `%`
- keywords
- identifiers
- strings
- multiline strings
- integers
- `[[`, `]]`
- `{`, `}`
- `(`, `)`
- `[`, `]`
- `,`, `;`, `=`
- `..`

Treat comments and whitespace as trivia.

## Parser
Use recursive descent:
- top-level declaration parser dispatches on `%` + keyword
- property lists are generic and reusable
- values parse with precedence:
  1. string / multiline / integer / boolean
  2. range
  3. list
  4. record
  5. call expression
  6. atom

One practical note: parse `integer ".." integer` as a special case before treating integer as a scalar.

---

# 13. Small Example That Fits the Grammar

```txt
%metadata [[
    package = "azma.examples";
    version = "0.1";
]];

%api PacketAPI {
    %symbol parse_packet [[
        link_name = "parse_packet";
        returns = "int";
        params = [
            { name = "data", type = "const uint8_t*" },
            { name = "size", type = "size_t" }
        ];
        tags = ["core", "parser"];
    ]];
}

%section fuzz PacketFuzz {
    %input-data PacketInput {
        schema = record {
            data: bytes(min=0, max=4096);
        };
    }

    %oracle ParseDoesNotCrash [[
        kind = crash_free;
    ]];

    %harness ParseHarness [[
        target = symbol("parse_packet");
        input = "PacketInput";
        oracle = "ParseDoesNotCrash";
    ]];

    %seed-corpus InitialSeeds [[
        paths = [glob!("corpus/packets/*")];
    ]];
}
```

