#include "../Polyexec.hpp"
#include "../AzmaTest/AzmaTest.hpp"

using namespace polyexec;
using namespace azmatest;

TEST_SUITE("ProcessTask") {
    TEST_CASE("make_process_task creates task") {
        auto task = make_process_task("echo", {"hello"});
        ASSERT_NE(task, nullptr);
    }
    
    TEST_CASE("process task executes successfully") {
        auto task = make_process_task("echo", {"test"});
        
        std::thread t([&task]() {
            task->execute({});
        });
        
        auto result = task->wait();
        t.join();
        
        ASSERT_EQ(result.state, TaskState::Completed);
        ASSERT_EQ(result.exit_code, 0);
    }
    
    TEST_CASE("process task captures stdout") {
        auto task = make_typed_process_task("echo", {"hello"});
        
        std::thread t([&task]() {
            task.base_->execute({});
        });
        
        auto result = task.get();
        t.join();
        
        ASSERT_TRUE(result.stdout_data.find("hello") != std::string::npos);
    }
    
    TEST_CASE("failed process returns non-zero exit code") {
        auto task = make_process_task("false");
        
        std::thread t([&task]() {
            task->execute({});
        });
        
        auto result = task->wait();
        t.join();
        
        ASSERT_EQ(result.state, TaskState::Failed);
        ASSERT_NE(result.exit_code, 0);
    }
    
    TEST_CASE("retry policy retries on failure") {
        auto task = make_process_task("false");
        
        RetryPolicy retry;
        retry.max_attempts = 3;
        retry.initial_delay = std::chrono::milliseconds(10);
        task->set_retry_policy(retry);
        
        std::thread t([&task]() {
            task->execute({});
        });
        
        auto result = task->wait();
        t.join();
        
        ASSERT_EQ(result.state, TaskState::Failed);
        // Should have attempted 3 times
    }
    
    TEST_CASE("task cancellation works") {
        auto task = make_process_task("sleep", {"10"});
        task->cancel();
        
        ASSERT_EQ(task->state(), TaskState::Cancelled);
    }
    
    TEST_CASE("priority can be set") {
        auto task = make_process_task("echo", {"test"});
        task->set_priority(Priority::High);
        
        ASSERT_EQ(task->priority(), Priority::High);
    }
}

int main() {
    return azmatest::run_all_tests();
}
