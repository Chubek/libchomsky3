# Polyflow User Guide

## Getting Started

### 1. Basic Task Graph

```cpp
#include "Polyflow.hpp"

int main() {
    using namespace polyflow;
    
    task_graph graph;
    auto load = graph.add_task("load");
    auto process = graph.add_task("process");
    auto save = graph.add_task("save");
    
    graph.add_dependency(load, process);
    graph.add_dependency(process, save);
    
    execution_context ctx(4);
    ctx.run_all(graph);
    
    return 0;
}
```

### 2. Process Execution

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    auto task = make_typed_process_task("ls", {"-la"});
    
    std::thread t([&task]() {
        task.base_->execute({});
    });
    
    auto result = task.get();
    std::cout << result.stdout_data;
    
    t.join();
    return 0;
}
```

### 3. Error Handling

```cpp
auto task = make_process_task("risky_command");

RetryPolicy retry;
retry.max_attempts = 3;
retry.initial_delay = std::chrono::milliseconds(100);
retry.backoff_multiplier = 2.0;

task->set_retry_policy(retry);

std::thread t([&task]() {
    task->execute({});
});

auto result = task->wait();
t.join();

if (result.is_success()) {
    std::cout << "Success!\n";
} else {
    std::cerr << "Failed: " << result.error_msg.value_or("unknown") << "\n";
}
```

## Common Patterns

### Fan-Out / Fan-In

```cpp
task_graph graph;

auto source = graph.add_task("source");

// Fan-out
auto worker1 = graph.add_task("worker1");
auto worker2 = graph.add_task("worker2");
auto worker3 = graph.add_task("worker3");

graph.add_dependency(source, worker1);
graph.add_dependency(source, worker2);
graph.add_dependency(source, worker3);

// Fan-in
auto sink = graph.add_task("sink");
graph.add_dependency(worker1, sink);
graph.add_dependency(worker2, sink);
graph.add_dependency(worker3, sink);

execution_context ctx(4);
ctx.run_all(graph);
```

### Pipeline

```cpp
auto stage1 = make_typed_process_task("extract");
auto stage2 = stage1.then([](ProcessResult r) {
    // Transform
    return process(r.stdout_data);
});
auto stage3 = stage2.then([](std::string s) {
    // Load
    return save(s);
});

std::thread t([&stage1]() { stage1.base_->execute({}); });
auto final = stage3.get();
t.join();
```

### Conditional Execution

```cpp
auto check = make_typed_process_task("test", {"-f", "data.txt"});

std::thread t([&check]() { check.base_->execute({}); });
auto result = check.get();
t.join();

if (result.exit_code == 0) {
    // File exists, proceed
    auto process = make_process_task("process_data");
    // ...
} else {
    // File missing, download
    auto download = make_process_task("download_data");
    // ...
}
```

## Best Practices

### 1. Validate Graphs

```cpp
if (graph.has_cycle()) {
    std::cerr << "Cycle detected in task graph!\n";
    return 1;
}
```

### 2. Set Timeouts

```cpp
task->set_timeout(std::chrono::seconds(30));
```

### 3. Use Priorities

```cpp
critical_task->set_priority(Priority::Critical);
background_task->set_priority(Priority::Background);
```

### 4. Monitor Production

```bash
polyflow-monitor --id <taskflow_id>
```

### 5. Handle Cancellation

```cpp
auto task = make_process_task("long_running");

std::thread canceller([&task]() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    task->cancel();
});

std::thread executor([&task]() {
    task->execute({});
});

auto result = task->wait();

if (result.state == TaskState::Cancelled) {
    std::cout << "Task cancelled\n";
}

canceller.join();
executor.join();
```

## Troubleshooting

### Deadlock Detection

```cpp
if (graph.has_cycle()) {
    std::cerr << "Deadlock: circular dependency detected\n";
}
```

### Task Hangs

- Check for missing dependencies
- Verify timeout settings
- Use monitor to inspect state

### Performance Issues

- Tune worker count: `std::thread::hardware_concurrency()`
- Adjust task granularity (10-100ms per task)
- Use priorities to prevent starvation

### Memory Pressure

- Limit concurrent tasks
- Use resource limits
- Monitor with `polyflow-monitor`

## Advanced Usage

See [Advanced Topics](docs/manual/05_advanced_topics.md) for:
- Custom task types
- Work-stealing internals
- Telemetry integration
- Coroutine integration
- Performance tuning
