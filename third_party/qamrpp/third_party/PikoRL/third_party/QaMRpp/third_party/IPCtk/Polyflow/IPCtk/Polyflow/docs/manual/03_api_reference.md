# API Reference

## Polyflow.hpp

### task_graph

```cpp
class task_graph {
public:
    task_handle add_task(std::string name);
    void add_dependency(task_handle from, task_handle to);
    std::vector<task_handle> schedule();
    bool has_cycle() const;
};
```

**Methods:**
- `add_task(name)`: Create a new task node
- `add_dependency(from, to)`: Add edge from → to
- `schedule()`: Return topological execution order
- `has_cycle()`: Check for cycles

### scheduler

```cpp
class scheduler {
public:
    explicit scheduler(int num_workers);
    void enqueue(std::shared_ptr<TaskBase> task);
    void start();
    void stop();
    void wait_all();
};
```

**Methods:**
- `scheduler(num_workers)`: Create scheduler with N worker threads
- `enqueue(task)`: Add task to priority queue
- `start()`: Start worker threads
- `stop()`: Stop all workers
- `wait_all()`: Block until all tasks complete

### execution_context

```cpp
class execution_context {
public:
    explicit execution_context(int num_workers);
    void run_all(const task_graph& graph);
    void wait(task_handle h);
    void cancel(task_handle h);
};
```

**Methods:**
- `execution_context(num_workers)`: Create context with N workers
- `run_all(graph)`: Execute entire task graph
- `wait(h)`: Block until task completes
- `cancel(h)`: Cancel task if not started

### task_metadata

```cpp
struct task_metadata {
    std::string name;
    Priority priority = Priority::Normal;
    std::optional<std::chrono::milliseconds> timeout;
    RetryPolicy retry_policy;
};
```

**Fields:**
- `name`: Human-readable task name
- `priority`: Scheduling priority
- `timeout`: Execution time limit
- `retry_policy`: Retry configuration

## Polyexec.hpp

### ProcessBuilder

```cpp
class ProcessBuilder {
public:
    ProcessBuilder(std::string cmd);
    ProcessBuilder& args(std::vector<std::string> a);
    ProcessBuilder& env(std::unordered_map<std::string, std::string> e);
    ProcessBuilder& limits(ResourceLimits l);
    ProcessResult run();
};
```

**Methods:**
- `ProcessBuilder(cmd)`: Create builder for command
- `args(a)`: Set command arguments
- `env(e)`: Set environment variables
- `limits(l)`: Set resource limits
- `run()`: Execute and return result

### ProcessResult

```cpp
struct ProcessResult {
    int exit_code;
    std::string stdout_data;
    std::string stderr_data;
    bool is_success() const;
};
```

**Fields:**
- `exit_code`: Process exit code
- `stdout_data`: Captured stdout
- `stderr_data`: Captured stderr
- `is_success()`: Returns `exit_code == 0`

### Task<T>

```cpp
template<typename T>
class Task {
public:
    T get();
    std::optional<T> try_get();
    void cancel();
    TaskState state() const;
    
    template<typename Fn>
    auto then(Fn&& fn) -> Task<std::invoke_result_t<Fn, T>>;
};
```

**Methods:**
- `get()`: Block until task completes, return result
- `try_get()`: Non-blocking result retrieval
- `cancel()`: Cancel task
- `state()`: Get current task state
- `then(fn)`: Chain continuation

### make_process_task

```cpp
std::shared_ptr<ProcessTask> make_process_task(
    const std::string& cmd,
    const std::vector<std::string>& args = {},
    const std::unordered_map<std::string, std::string>& env = {}
);
```

**Parameters:**
- `cmd`: Command to execute
- `args`: Command arguments
- `env`: Environment variables

**Returns:** Shared pointer to ProcessTask

### make_typed_process_task

```cpp
Task<ProcessResult> make_typed_process_task(
    const std::string& cmd,
    const std::vector<std::string>& args = {},
    const std::unordered_map<std::string, std::string>& env = {}
);
```

**Parameters:**
- `cmd`: Command to execute
- `args`: Command arguments
- `env`: Environment variables

**Returns:** Typed task wrapper

### RetryPolicy

```cpp
struct RetryPolicy {
    int max_attempts = 1;
    std::chrono::milliseconds initial_delay{100};
    double backoff_multiplier = 2.0;
    std::chrono::milliseconds max_delay{30000};
    
    std::chrono::milliseconds delay_for_attempt(int attempt) const;
};
```

**Fields:**
- `max_attempts`: Maximum retry attempts
- `initial_delay`: Initial delay before first retry
- `backoff_multiplier`: Exponential backoff factor
- `max_delay`: Maximum delay cap

### ResourceLimits

```cpp
struct ResourceLimits {
    std::optional<size_t> max_memory_bytes;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<int> cpu_affinity;
};
```

**Fields:**
- `max_memory_bytes`: Memory limit
- `timeout`: Execution timeout
- `cpu_affinity`: CPU core affinity

### Future<T> / Promise<T>

```cpp
template<typename T>
class Future {
public:
    T get();
    bool is_ready() const;
    
    template<typename U>
    Future<U> then(std::function<U(T)> fn);
};

template<typename T>
class Promise {
public:
    Future<T> get_future();
    void set_value(T value);
    void set_exception(std::exception_ptr e);
};
```

**Future Methods:**
- `get()`: Block until value available
- `is_ready()`: Check if value ready
- `then(fn)`: Chain continuation

**Promise Methods:**
- `get_future()`: Get associated future
- `set_value(value)`: Fulfill promise
- `set_exception(e)`: Reject promise

## Enumerations

### TaskState

```cpp
enum class TaskState : std::uint8_t {
    TS_Pending,
    TS_Ready,
    TS_Queued,
    TS_Running,
    TS_Completed,
    TS_Failed,
    TS_Cancelled
};
```

### Priority

```cpp
enum class Priority : std::uint8_t {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Background = 4
};
```

## Exceptions

```cpp
class constraint_violation : public std::runtime_error {};
class deadlock_error : public std::runtime_error {};
class timeout_error : public std::runtime_error {};
class task_not_found : public std::runtime_error {};
```

## Monitor CLI

### polyflow-monitor

```bash
# Monitor by taskflow ID
polyflow-monitor --id <taskflow_id>

# Monitor by process PID
polyflow-monitor --pid <process_pid>

# Discover active taskflows
polyflow-monitor --discover
```

**Keyboard Controls:**
- `q`: Quit
- `r`: Refresh immediately
- `w`: Show worker statistics
- `d`: Show dependency graph view
- `h`: Help screen
