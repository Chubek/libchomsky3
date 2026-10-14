# Polyflow

**Polyflow** is a modern C++20 header-only library for graph-based task concurrency.

## Features

- **Task Graph Orchestration**: Define task dependencies as a directed acyclic graph (DAG)
- **Work-Stealing Scheduler**: Efficient multi-threaded execution with LIFO local / FIFO steal semantics
- **Process Integration**: Execute external processes as tasks with resource limits and retry policies
- **Type-Safe I/O**: Strongly-typed task inputs and outputs using ZethaMEM values
- **Real-Time Monitoring**: Standalone TUI monitor for observing live taskflow execution
- **Cancellation & Timeouts**: Cooperative cancellation and timeout enforcement
- **Continuation Chaining**: Compose tasks with `.then()` for functional-style pipelines

## Quick Start

```cpp
#include "Polyflow.hpp"
#include "Polyexec.hpp"

int main() {
    using namespace polyflow;
    
    // Create task graph
    task_graph graph;
    auto t1 = graph.add_task("load_data");
    auto t2 = graph.add_task("process");
    auto t3 = graph.add_task("save");
    
    graph.add_dependency(t1, t2);
    graph.add_dependency(t2, t3);
    
    // Execute with 8 worker threads
    execution_context ctx(8);
    ctx.run_all(graph);
    
    return 0;
}
```

## Installation

Polyflow is header-only. Simply include the headers:

```cpp
#include "Polyflow.hpp"
#include "Polyexec.hpp"
```

Or use CMake:

```bash
mkdir build && cd build
cmake ..
make
make test
```

## Process Tasks

```cpp
using namespace polyexec;

auto task = make_typed_process_task("echo", {"hello"});

std::thread t([&task]() {
    task.base_->execute({});
});

auto result = task.get();
std::cout << result.stdout_data; // "hello\n"

t.join();
```

## Retry Policies

```cpp
auto task = make_process_task("curl", {"https://api.example.com"});

RetryPolicy retry;
retry.max_attempts = 5;
retry.initial_delay = std::chrono::milliseconds(100);
retry.backoff_multiplier = 2.0;

task->set_retry_policy(retry);
```

## Continuation Chaining

```cpp
auto task = make_typed_process_task("cat", {"/etc/hostname"});

auto upper = task.then([](ProcessResult pr) -> std::string {
    std::string s = pr.stdout_data;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
});

std::thread t([&task]() { task.base_->execute({}); });
std::cout << upper.get();
t.join();
```

## Monitoring

```bash
# Terminal 1: Run your application
./my_polyflow_app

# Terminal 2: Monitor live execution
polyflow-monitor --id tf_12345

# Or discover all active taskflows
polyflow-monitor --discover
```

## Documentation

- [Introduction](docs/manual/01_introduction.md)
- [Architecture](docs/manual/02_architecture.md)
- [API Reference](docs/manual/03_api_reference.md)
- [Examples](docs/manual/04_examples.md)
- [Advanced Topics](docs/manual/05_advanced_topics.md)

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- POSIX threads (pthread)
- Linux/Unix environment

## Testing

```bash
# Run all tests
make test

# Or run individually
./test_polyflow
./test_polyexec
./tests/test_task_graph
./tests/test_scheduler
./tests/test_process_task
./tests/test_typed_task
./tests/test_future_promise
```

## Architecture

```
┌─────────────────────────────────────────┐
│         Application Code                │
└─────────────────┬───────────────────────┘
                  │
        ┌─────────▼─────────┐
        │   Polyflow.hpp    │  ← Task graph, scheduler
        │   Polyexec.hpp    │  ← Process tasks, typed I/O
        └─────────┬─────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
┌───▼────┐  ┌────▼─────┐  ┌───▼────┐
│Grafitt │  │ ZethaMEM │  │ IPCtk  │
│  DAG   │  │  Values  │  │  IPC   │
└────────┘  └──────────┘  └────────┘
```

## Components

- **Polyflow.hpp**: Main orchestration layer (task graph, scheduler, execution context)
- **Polyexec.hpp**: Execution layer (process tasks, typed wrappers, retry policies)
- **polyflow-monitor**: Standalone TUI monitor for real-time observation
- **Grafitt**: Graph storage and topological algorithms
- **ZethaMEM**: Typed value model for task I/O
- **IPCtk**: Inter-process communication for monitoring

## License

See [LICENSE](LICENSE) file for details.

## Contributing

Contributions welcome! Please ensure:
- Code compiles with C++20
- Tests pass (`make test`)
- Documentation updated
- Follows existing code style

## Status

**Stage 1 (Core)**: ✅ Complete
- Polyflow.hpp orchestration layer
- Polyexec.hpp execution layer
- Work-stealing scheduler
- Process task integration
- Typed task wrappers
- Future/Promise async
- Retry policies

**Stage 2 (Monitor)**: ✅ Complete
- PolyflowExternalMonitor.cpp TUI
- IPC telemetry integration
- Real-time task visualization
- Worker statistics view

**Documentation**: ✅ Complete
- Manual chapters (Introduction, Architecture, API, Examples, Advanced)
- Doxygen configuration
- API reference

**Testing**: ✅ Complete
- AzmaTest unit tests
- Integration tests
- Validation tests
