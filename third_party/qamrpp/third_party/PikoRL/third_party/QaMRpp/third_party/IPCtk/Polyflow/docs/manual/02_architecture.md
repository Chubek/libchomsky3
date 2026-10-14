# Architecture

## Overview

Polyflow consists of three layers:

1. **Graph Layer** (Grafitt): DAG storage, topological sorting, cycle detection
2. **Orchestration Layer** (Polyflow.hpp): Task graph, scheduler, execution context
3. **Execution Layer** (Polyexec.hpp): Process tasks, typed wrappers, retry policies

## Component Diagram

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

## Task Lifecycle

```
Pending → Ready → Queued → Running → Completed
                                   ↘ Failed
                                   ↘ Cancelled
```

### State Transitions

- **Pending**: Task created, dependencies not satisfied
- **Ready**: All dependencies completed, eligible for scheduling
- **Queued**: Enqueued in scheduler's priority queue
- **Running**: Executing on a worker thread
- **Completed**: Execution succeeded
- **Failed**: Execution failed (after retries)
- **Cancelled**: Cancelled before execution

## Scheduler Architecture

### Work-Stealing Deques

Each worker thread maintains a local deque:

- **Local push/pop**: LIFO (stack-like) for cache locality
- **Remote steal**: FIFO (queue-like) for load balancing

```
Worker 0:  [T1, T2, T3] ← local LIFO
Worker 1:  [T4, T5]     ← steal from Worker 0 (FIFO)
Worker 2:  []           ← idle, steal from others
```

### Priority Scheduling

Tasks are prioritized:

- **Critical** (0): System-critical tasks
- **High** (1): User-facing operations
- **Normal** (2): Default priority
- **Low** (3): Background processing
- **Background** (4): Lowest priority

Priority queue ensures high-priority tasks execute first.

## Dependency Resolution

### Topological Sorting

Before execution, the task graph is topologically sorted using Grafitt:

```cpp
auto order = grafitt::topological_sort(graph);
```

This ensures tasks execute in dependency order.

### Cycle Detection

Graphs are validated before execution:

```cpp
if (grafitt::detect_cycle(graph)) {
    throw deadlock_error("Cycle detected in task graph");
}
```

## Process Execution

### ProcessBuilder

External commands are executed via `ProcessBuilder`:

```cpp
auto result = ProcessBuilder("echo")
    .args({"hello"})
    .env({{"VAR", "value"}})
    .limits(ResourceLimits{.timeout = 5s})
    .run();
```

### Resource Limits

- **Memory**: Max memory allocation
- **Timeout**: Execution time limit
- **CPU affinity**: Pin to specific cores

## Type System

### Task I/O

Tasks use `zethamem::Value` for typed I/O:

```cpp
using TaskInput = std::unordered_map<std::string, zethamem::Value>;
using TaskOutput = std::unordered_map<std::string, zethamem::Value>;
```

Values support:
- `int64_t`
- `double`
- `bool`
- `std::string`

### Typed Task Wrappers

`Task<T>` provides type-safe result retrieval:

```cpp
Task<ProcessResult> task = make_typed_process_task("echo", {"hello"});
ProcessResult result = task.get(); // blocking
```

## Monitoring Architecture

### IPC Telemetry

The scheduler emits events via IPCtk:

- `TaskCreated`
- `TaskStateChanged`
- `TaskStarted`
- `TaskCompleted`
- `TaskFailed`
- `WorkerSteal`
- `QueueDepthUpdate`

### External Monitor

`polyflow-monitor` attaches to running taskflows:

```
Scheduler → IPCtk → /tmp/polyflow/<id>.ipc → Monitor TUI
```

Events are batched and non-blocking to avoid scheduler overhead.

## Error Propagation

Exceptions propagate through dependency chains:

```
Task A fails → Task B (depends on A) → Cancelled
```

Retry policies allow transient failures to recover.

## Concurrency Model

- **Thread-safe**: All public APIs are thread-safe
- **Lock-free**: Work-stealing uses atomic operations
- **Cooperative cancellation**: Tasks check cancellation tokens
- **No data races**: Immutable task graphs after validation

## Performance Considerations

- **PERF(codex)**: Deferred optimizations marked in code
- **Cache locality**: LIFO local deque improves cache hits
- **Minimal locking**: Per-worker deques reduce contention
- **Batch telemetry**: Monitor events are batched to reduce overhead
