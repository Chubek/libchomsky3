# IPCtk Manual

## Chapter 1: Introduction and Goals
IPCtk is a header-only C++20 IPC toolkit with language, runtime, and serialization-extension layers.
Functional now: native DSL, core IR, compile pipeline, runtime mock channel.
Placeholder: Serde integration and some platform wrappers.

## Chapter 2: Building and Including `IPCtk.hpp`
Include local sibling headers:
```cpp
#include "IPCtk.hpp"
```
No separate build step is required.

## Chapter 3: `DSLUtils.hpp` Integration Overview
IPCtk reuses `dsl::DSL`, `dsl::Result`, `dsl::Pipeline`, `dsl::ASTNode`, parser combinators, and rewrite utilities.
Functional now: these are wired into parser/validation/compile flow.

## Chapter 4: IPC Runtime Helpers and QoL APIs
Use `runtime::Endpoint`, `runtime::ChannelConfig`, `runtime::Channel`, and `open_channel`.
Functional now: raw byte send/receive with max-frame checks.

## Chapter 5: Native System IPC Wrappers
`runtime` is intentionally conservative and currently mock-oriented.
Placeholder: OS handle wrappers for pipes, unix sockets, and named pipes are future patches.

## Chapter 6: Placeholder Serde/Serialization Design
`serde::NoSerde`, `serde::CodecPolicy`, `serde::encode/decode` provide stable seams.
Functional now: explicit unavailability errors.
Placeholder: real codec hooks expected from future `SerdeTk.hpp`.

## Chapter 7: IPC-L Overview
IPC-L models resources, operations, and pipe chains (`->`) for protocol composition.
Functional now: parsable subset and native builder support.

## Chapter 8: IPC-L Textual Syntax
Core forms:
- `kind name = initializer;`
- `pipe name = op(a) -> op(b);`
Functional now: resources and single-line pipe parsing to `ir::Program`.

## Chapter 9: IPC-L Native C++ DSL
Core forms:
- `socket("s") = "tcp.listen(...)"`
- `pipe("p") = recv(s) >> send(s)`
Functional now: resource/pipe creation and step composition.

## Chapter 10: ITKD Overview
ITKD describes backend target, mappings, and emission rules.
Functional now: textual target parse and native `BackendBuilder` producing `BackendSpec`.

## Chapter 11: ITKD Textual Backend Syntax
Minimal textual example:
```
target python
capability pubsub
```
Functional now: identifier target parsing.
Placeholder: full rule grammar.

## Chapter 12: ITKD Native C++ Backend DSL
Use `itkd::backend().target("python").capability("pubsub").build()`.
Functional now: same `BackendSpec` IR as textual frontend.

## Chapter 13: End-to-End Compile Pipeline
Pipeline stages: parse -> validate -> normalize -> emit.
Functional now: `compile(program, backend_spec)` and validation checks.
Placeholder: rich lowering and template expansion.

## Chapter 14: Testing, Extension, and Future Work
Recommended tests:
- IPC-L/ITKD parsing
- native DSL construction
- compile diagnostics via `dsl::Result`
- runtime smoke checks
Future work: richer combinator grammars, OS IPC adapters, SerdeTk codecs.
