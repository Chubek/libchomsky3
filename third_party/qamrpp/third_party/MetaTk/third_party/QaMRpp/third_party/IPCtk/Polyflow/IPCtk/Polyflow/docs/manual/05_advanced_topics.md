# Advanced Topics

## Work-Stealing Internals

### Deque Implementation

Each worker maintains a double-ended queue (deque):

```cpp
struct WorkerDeque {
    std::deque<std::shared_ptr<TaskBase>> tasks;
    std::mutex mutex;
    
    // Local operations (LIFO)
    void push_local(std::shared_ptr<TaskBase> task) {
        std::lock_guard lock(mutex);
        tasks.push_back(task); // Push to back
    }
    
    std::optional<std::shared_ptr<TaskBase>> pop_local() {
        std::lock_guard lock(mutex);
        if (tasks.empty()) return std::nullopt;
        auto task = tasks.back(); // Pop from back (LIFO)
        tasks.pop_back();
        return task;
    }
    
    // Remote steal (FIFO)
    std::optional<std::shared_ptr<TaskBase>> steal() {
        std::lock_guard lock(mutex);
        if (tasks.empty()) return std::nullopt;
        auto task = tasks.front(); // Steal from front (FIFO)
        tasks.pop_front();
        return task;
    }
};
```

### Steal Strategy

Workers attempt to steal when idle:

1. Check local deque
2. If empty, randomly select victim worker
3. Attempt to steal from victim's front
4. Repeat until task found or all workers checked

## Custom Task Types

### Implementing TaskBase

```cpp
class CustomTask : public polyexec::TaskBase {
    std::function<void()> fn_;
    
public:
    CustomTask(std::function<void()> fn) : fn_(std::move(fn)) {}
    
    void execute(const TaskInput& input) override {
        set_state(TaskState::TS_Running);
        
        try {
            fn_();
            
            TaskResult result;
            result.state = TaskState::TS_Completed;
            set_result(result);
        } catch (...) {
            set_exception(std::current_exception());
        }
    }
};
```

### Usage

```cpp
auto task = std::make_shared<CustomTask>([]() {
    std::cout << "Custom task executing\n";
});

scheduler sched(4);
sched.enqueue(task);
sched.start();
sched.wait_all();
```

## Error Handling Strategies

### Fail-Fast

Default behavior: first failure stops dependent tasks.

```cpp
task_graph graph;
auto t1 = graph.add_task("may_fail");
auto t2 = graph.add_task("depends_on_t1");
graph.add_dependency(t1, t2);

// If t1 fails, t2 is cancelled
```

### Retry with Backoff

Transient failures can be retried:

```cpp
RetryPolicy retry;
retry.max_attempts = 5;
retry.initial_delay = std::chrono::milliseconds(100);
retry.backoff_multiplier = 2.0; // 100ms, 200ms, 400ms, 800ms, 1600ms

task->set_retry_policy(retry);
```

### Graceful Degradation

Use `try_get()` for non-blocking checks:

```cpp
auto result = task.try_get();
if (result && result->is_success()) {
    // Use result
} else {
    // Fallback logic
}
```

## Performance Tuning

### Worker Count

Rule of thumb: `num_workers = std::thread::hardware_concurrency()`

```cpp
int optimal = std::thread::hardware_concurrency();
execution_context ctx(optimal);
```

### Task Granularity

- **Too fine-grained**: Scheduling overhead dominates
- **Too coarse-grained**: Poor load balancing

Target: 10-100ms per task for optimal throughput.

### Priority Tuning

Use priorities to ensure critical tasks execute first:

```cpp
task->set_priority(Priority::Critical); // Preempts lower priorities
```

### Memory Pressure

Limit concurrent tasks to control memory:

```cpp
scheduler sched(4); // Only 4 tasks run concurrently
```

## Telemetry Integration

### Custom Event Handlers

```cpp
class TelemetryScheduler : public scheduler {
    void on_task_start(task_handle h) override {
        // Emit metric
        metrics::counter("tasks_started").increment();
    }
    
    void on_task_complete(task_handle h, const TaskResult& r) override {
        metrics::histogram("task_duration_ms").observe(r.execution_time_ms);
    }
};
```

### IPC Event Schema

Monitor events are serialized as:

```cpp
struct MonitorEvent {
    EventType type;
    std::string taskflow_id;
    std::string task_id;
    TaskState task_state;
    int worker_id;
    std::chrono::steady_clock::time_point timestamp;
    int64_t execution_time_ms;
};
```

## Debugging

### Enable Verbose Logging

```cpp
#define POLYFLOW_DEBUG 1
#include "Polyflow.hpp"
```

### Deadlock Detection

```cpp
if (graph.has_cycle()) {
    auto cycle = grafitt::find_cycle(graph);
    std::cerr << "Cycle: ";
    for (auto h : cycle) {
        std::cerr << h.id << " -> ";
    }
    std::cerr << "\n";
}
```

### Task State Inspection

```cpp
std::cout << "Task state: " << polyflow::to_string(task->state()) << "\n";
```

## Thread Safety

### Immutable Graphs

Task graphs are immutable after validation:

```cpp
task_graph graph;
// ... add tasks and dependencies ...
graph.freeze(); // No further modifications allowed
```

### Concurrent Access

All public APIs are thread-safe:

```cpp
// Safe from multiple threads
ctx.wait(task1);
ctx.wait(task2);
ctx.cancel(task3);
```

## Integration Patterns

### With Coroutines (C++20)

```cpp
#include <coroutine>

struct TaskAwaiter {
    std::shared_ptr<TaskBase> task;
    
    bool await_ready() { return task->state() == TaskState::TS_Completed; }
    void await_suspend(std::coroutine_handle<> h) {
        // Resume coroutine when task completes
    }
    TaskResult await_resume() { return task->wait(); }
};

Task<int> async_compute() {
    auto task = make_process_task("compute");
    co_await TaskAwaiter{task};
    co_return 42;
}
```

### With Ranges (C++20)

```cpp
#include <ranges>

auto results = tasks 
    | std::views::transform([](auto& t) { return t.get(); })
    | std::views::filter([](auto& r) { return r.is_success(); });
```

## Benchmarking

### Measure Throughput

```cpp
auto start = std::chrono::steady_clock::now();

execution_context ctx(8);
ctx.run_all(graph);

auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Throughput: " << (graph.size() * 1000.0 / duration.count()) 
          << " tasks/sec\n";
```

### Profile Worker Utilization

```bash
polyflow-monitor --id <taskflow_id>
# Press 'w' to view worker statistics
```

## Best Practices

1. **Validate graphs before execution**: Use `has_cycle()` to catch errors early
2. **Set timeouts for external processes**: Prevent hung tasks
3. **Use retry policies for network I/O**: Handle transient failures
4. **Monitor production taskflows**: Use `polyflow-monitor` for observability
5. **Tune worker count**: Match hardware concurrency
6. **Batch small tasks**: Reduce scheduling overhead
7. **Use priorities judiciously**: Avoid starvation of low-priority tasks
8. **Test with AzmaTest**: Validate task logic in isolation

## Limitations

- **No dynamic graph modification**: Graphs are immutable after validation
- **POSIX-only**: Process execution requires Unix-like environment
- **No distributed execution**: Single-machine only
- **No GPU support**: CPU-only task execution

## Future Enhancements

- **PERF(codex)**: Lock-free work-stealing deques
- **PERF(codex)**: NUMA-aware task placement
- **PERF(codex)**: Task fusion for small tasks
- **PERF(codex)**: Adaptive worker scaling
