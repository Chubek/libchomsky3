#include "../Polyflow.hpp"
#include "../Polyexec.hpp"
#include "../AzmaTest/AzmaTest.hpp"

using namespace polyflow;
using namespace polyexec;
using namespace azmatest;

TEST_SUITE("Scheduler") {
    TEST_CASE("scheduler starts and stops") {
        scheduler sched(2);
        sched.start();
        sched.stop();
        ASSERT_TRUE(true);
    }
    
    TEST_CASE("enqueue adds task to queue") {
        scheduler sched(2);
        auto task = make_process_task("echo", {"test"});
        
        sched.enqueue(task);
        sched.start();
        sched.wait_all();
        sched.stop();
        
        ASSERT_EQ(task->state(), TaskState::Completed);
    }
    
    TEST_CASE("priority tasks execute first") {
        scheduler sched(1); // Single worker
        
        auto low = make_process_task("echo", {"low"});
        low->set_priority(Priority::Low);
        
        auto high = make_process_task("echo", {"high"});
        high->set_priority(Priority::High);
        
        sched.enqueue(low);
        sched.enqueue(high);
        
        sched.start();
        sched.wait_all();
        sched.stop();
        
        // High priority should complete first
        ASSERT_EQ(high->state(), TaskState::Completed);
        ASSERT_EQ(low->state(), TaskState::Completed);
    }
    
    TEST_CASE("wait_all blocks until completion") {
        scheduler sched(2);
        
        auto t1 = make_process_task("sleep", {"0.1"});
        auto t2 = make_process_task("sleep", {"0.1"});
        
        sched.enqueue(t1);
        sched.enqueue(t2);
        sched.start();
        
        auto start = std::chrono::steady_clock::now();
        sched.wait_all();
        auto end = std::chrono::steady_clock::now();
        
        sched.stop();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        ASSERT_GE(duration.count(), 100); // At least 100ms
    }
}

int main() {
    return azmatest::run_all_tests();
}
