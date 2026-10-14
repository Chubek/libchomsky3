#include "Polyexec.hpp"
#include <iostream>
#include <cassert>

int main() {
    using namespace polyexec;
    
    std::cout << "=== Polyexec Stage 1.2 Validation ===\n\n";
    
    // Test 1: Basic process task
    std::cout << "[1] Basic process task (echo)...\n";
    auto task1 = make_typed_process_task("echo", {"hello"});
    
    // Execute in background thread
    std::thread t1([&task1]() {
        task1.base_->execute({});
    });
    
    auto result1 = task1.get();
    t1.join();
    
    assert(result1.exit_code == 0);
    assert(result1.stdout_data.find("hello") != std::string::npos);
    std::cout << "    ✓ stdout: " << result1.stdout_data;
    
    // Test 2: Retry policy
    std::cout << "[2] Retry policy (failing command)...\n";
    auto task2 = make_process_task("false");
    RetryPolicy retry;
    retry.max_attempts = 3;
    retry.initial_delay = std::chrono::milliseconds(10);
    task2->set_retry_policy(retry);
    
    std::thread t2([&task2]() {
        task2->execute({});
    });
    
    auto result2 = task2->wait();
    t2.join();
    
    assert(result2.state == TaskState::Failed);
    std::cout << "    ✓ Failed after retries as expected\n";
    
    // Test 3: Cancellation
    std::cout << "[3] Task cancellation...\n";
    auto task3 = make_process_task("sleep", {"10"});
    task3->cancel();
    assert(task3->state() == TaskState::Cancelled);
    std::cout << "    ✓ Cancelled before execution\n";
    
    // Test 4: Priority
    std::cout << "[4] Priority setting...\n";
    auto task4 = make_process_task("echo", {"test"});
    task4->set_priority(Priority::High);
    assert(task4->priority() == Priority::High);
    std::cout << "    ✓ Priority set to High\n";
    
    // Test 5: Future/Promise
    std::cout << "[5] Future/Promise async...\n";
    Promise<int> promise;
    auto future = promise.get_future();
    
    std::thread t5([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        promise.set_value(42);
    });
    
    int value = future.get();
    t5.join();
    assert(value == 42);
    std::cout << "    ✓ Future resolved with value: " << value << "\n";
    
    // Test 6: Continuation chaining
    std::cout << "[6] Continuation chaining (.then)...\n";
    auto task6 = make_typed_process_task("echo", {"world"});
    
    std::thread t6([&task6]() {
        task6.base_->execute({});
    });
    
    auto chained = task6.then([](ProcessResult pr) -> std::string {
        return "Processed: " + pr.stdout_data;
    });
    
    auto final_result = chained.get();
    t6.join();
    
    assert(final_result.find("Processed:") != std::string::npos);
    assert(final_result.find("world") != std::string::npos);
    std::cout << "    ✓ Chained result: " << final_result;
    
    std::cout << "\n=== All Polyexec tests passed ===\n";
    return 0;
}
