# Chapter 1 - Vision and Constraints

ZethaMEM is a header-only, in-memory, schema-first database for C++. The design target is embeddability.

The library intentionally avoids persistence, mmap, runtime plugins, and transactional engines. This keeps integration simple for tools, games, local apps, and educational projects.

Key outcomes:
- one include (`ZethaMEM.hpp`)
- deterministic schema validation
- tiny query DSL for CRUD behavior
- parser and runtime separation
