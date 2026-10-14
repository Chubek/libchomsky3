# Chapter-1: Project Overview

## Components

- `IPCtk.hpp`: public C++ API surface.
- `DSLUtils.hpp`: DSL support utilities.
- IPC-L: protocol/flow DSL.
- ITKD: backend mapping/generation DSL.
- `bindings/`: SWIG interface and generation script.
- `dest/`: backend destination templates (`C`, `Python`, `Ruby`).
- `examples/`: C++ and DSL examples.
- `tests/unit/`: deterministic regression tests.
- `tests/fuzz/`: parser/runtime robustness fuzz harnesses.

## Relationship Model

- IPC-L describes semantic transport/process graphs.
- ITKD maps semantic graphs to target artifacts.
- `dest/*.itkd` defines per-language rendering contracts.
- C++ API and DSL flows must remain behaviorally aligned.

## Audience

- integrators building IPC services;
- language/binding maintainers;
- backend template authors;
- verification and fuzzing engineers.
