# Introduction

## What is Polyflow?

Polyflow is a C++20 header-only library for graph-based task concurrency. It enables developers to express complex computational workflows as directed acyclic graphs (DAGs) and execute them efficiently across multiple threads.

## Key Features

- **Header-only**: No separate compilation or linking required
- **Modern C++20**: Leverages concepts, ranges, and coroutines
- **Zero-cost abstractions**: Minimal runtime overhead
- **Work-stealing scheduler**: Automatic load balancing across worker threads
- **Process integration**: Execute external commands as tasks
- **Type-safe I/O**: Strongly-typed task inputs and outputs
- **Real-time monitoring**: Standalone TUI for observing live execution
- **Cancellation support**: Cooperative task cancellation
- **Retry policies**: Exponential backoff for transient failures

## Design Principles

1. **Explicit dependencies**: Task relationships are declared upfront
2. **Immutable graphs**: Task graphs are validated before execution
3. **Fail-fast**: Errors propagate immediately through dependency chains
4. **Observable**: All state transitions are visible via monitoring
5. **Composable**: Tasks can be chained with `.then()` for functional pipelines

## Use Cases

- **Data pipelines**: ETL workflows with complex dependencies
- **Build systems**: Parallel compilation with dependency tracking
- **Scientific computing**: Multi-stage simulations with checkpointing
- **DevOps automation**: Orchestrate deployment steps with rollback
- **Testing frameworks**: Parallel test execution with resource constraints

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- POSIX threads (pthread)
- Linux/Unix environment (for process execution and IPC)

## Installation

Polyflow is header-only. Simply include the headers:

```cpp
#include "Polyflow.hpp"
#include "Polyexec.hpp"
```

Or use CMake:

```cmake
add_subdirectory(polyflow)
target_link_libraries(your_target PRIVATE polyflow)
```

## Next Steps

- Read the [Architecture](02_architecture.md) overview
- Explore the [API Reference](03_api_reference.md)
- Study the [Examples](04_examples.md)
- Learn [Advanced Topics](05_advanced_topics.md)
