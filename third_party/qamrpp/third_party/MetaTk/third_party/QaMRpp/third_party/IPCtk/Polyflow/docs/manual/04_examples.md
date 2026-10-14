# Examples

## Basic Task Graph

```cpp
#include "Polyflow.hpp"

int main() {
    using namespace polyflow;
    
    // Create task graph
    task_graph graph;
    auto load = graph.add_task("load_data");
    auto process = graph.add_task("process_data");
    auto save = graph.add_task("save_results");
    
    // Define dependencies
    graph.add_dependency(load, process);
    graph.add_dependency(process, save);
    
    // Execute
    execution_context ctx(4); // 4 worker threads
    ctx.run_all(graph);
    
    return 0;
}
```

## Process Task Execution

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    // Create process task
    auto task = make_typed_process_task("ls", {"-la", "/tmp"});
    
    // Execute in background
    std::thread t([&task]() {
        task.base_->execute({});
    });
    
    // Wait for result
    auto result = task.get();
    std::cout << "Exit code: " << result.exit_code << "\n";
    std::cout << "Output:\n" << result.stdout_data;
    
    t.join();
    return 0;
}
```

## Retry Policy

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    auto task = make_process_task("curl", {"https://api.example.com/data"});
    
    // Configure retry policy
    RetryPolicy retry;
    retry.max_attempts = 5;
    retry.initial_delay = std::chrono::milliseconds(100);
    retry.backoff_multiplier = 2.0;
    retry.max_delay = std::chrono::seconds(30);
    
    task->set_retry_policy(retry);
    
    // Execute with retries
    std::thread t([&task]() {
        task->execute({});
    });
    
    auto result = task->wait();
    t.join();
    
    if (result.is_success()) {
        std::cout << "Success after retries\n";
    } else {
        std::cout << "Failed: " << result.error_msg.value_or("unknown") << "\n";
    }
    
    return 0;
}
```

## Continuation Chaining

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    auto task = make_typed_process_task("cat", {"/etc/hostname"});
    
    // Chain transformations
    auto upper = task.then([](ProcessResult pr) -> std::string {
        std::string s = pr.stdout_data;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    });
    
    auto prefixed = upper.then([](std::string s) -> std::string {
        return "Hostname: " + s;
    });
    
    // Execute
    std::thread t([&task]() {
        task.base_->execute({});
    });
    
    std::cout << prefixed.get();
    t.join();
    
    return 0;
}
```

## Priority Scheduling

```cpp
#include "Polyflow.hpp"
#include "Polyexec.hpp"

int main() {
    using namespace polyflow;
    using namespace polyexec;
    
    auto critical_task = make_process_task("backup_db");
    critical_task->set_priority(Priority::Critical);
    
    auto normal_task = make_process_task("generate_report");
    normal_task->set_priority(Priority::Normal);
    
    auto background_task = make_process_task("cleanup_logs");
    background_task->set_priority(Priority::Background);
    
    scheduler sched(4);
    sched.enqueue(background_task);
    sched.enqueue(normal_task);
    sched.enqueue(critical_task); // Executes first
    
    sched.start();
    sched.wait_all();
    sched.stop();
    
    return 0;
}
```

## Resource Limits

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    auto task = make_process_task("heavy_computation");
    
    ResourceLimits limits;
    limits.max_memory_bytes = 1024 * 1024 * 512; // 512 MB
    limits.timeout = std::chrono::seconds(60);
    limits.cpu_affinity = 2; // Pin to core 2
    
    task->set_limits(limits);
    
    std::thread t([&task]() {
        task->execute({});
    });
    
    auto result = task->wait();
    t.join();
    
    return 0;
}
```

## Future/Promise

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    Promise<int> promise;
    auto future = promise.get_future();
    
    // Producer thread
    std::thread producer([&promise]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        promise.set_value(42);
    });
    
    // Consumer thread
    std::thread consumer([&future]() {
        int value = future.get(); // Blocks until ready
        std::cout << "Received: " << value << "\n";
    });
    
    producer.join();
    consumer.join();
    
    return 0;
}
```

## Cancellation

```cpp
#include "Polyexec.hpp"

int main() {
    using namespace polyexec;
    
    auto task = make_process_task("sleep", {"60"});
    
    // Cancel after 5 seconds
    std::thread canceller([&task]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        task->cancel();
    });
    
    std::thread executor([&task]() {
        task->execute({});
    });
    
    auto result = task->wait();
    
    if (result.state == TaskState::Cancelled) {
        std::cout << "Task was cancelled\n";
    }
    
    canceller.join();
    executor.join();
    
    return 0;
}
```

## Monitoring

```bash
# Terminal 1: Run your Polyflow application
./my_polyflow_app

# Terminal 2: Monitor the taskflow
polyflow-monitor --id tf_12345

# Or discover all active taskflows
polyflow-monitor --discover
```

## Complex Pipeline

```cpp
#include "Polyflow.hpp"
#include "Polyexec.hpp"

int main() {
    using namespace polyflow;
    
    task_graph graph;
    
    // Stage 1: Data ingestion
    auto fetch_users = graph.add_task("fetch_users");
    auto fetch_orders = graph.add_task("fetch_orders");
    
    // Stage 2: Processing
    auto join_data = graph.add_task("join_data");
    graph.add_dependency(fetch_users, join_data);
    graph.add_dependency(fetch_orders, join_data);
    
    // Stage 3: Analytics
    auto compute_metrics = graph.add_task("compute_metrics");
    auto generate_report = graph.add_task("generate_report");
    graph.add_dependency(join_data, compute_metrics);
    graph.add_dependency(join_data, generate_report);
    
    // Stage 4: Output
    auto upload_results = graph.add_task("upload_results");
    graph.add_dependency(compute_metrics, upload_results);
    graph.add_dependency(generate_report, upload_results);
    
    // Validate no cycles
    if (graph.has_cycle()) {
        std::cerr << "Cycle detected!\n";
        return 1;
    }
    
    // Execute
    execution_context ctx(8);
    ctx.run_all(graph);
    
    return 0;
}
```
