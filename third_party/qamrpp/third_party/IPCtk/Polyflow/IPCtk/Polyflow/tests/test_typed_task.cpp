#include "../Polyexec.hpp"
#include "../AzmaTest/AzmaTest.hpp"

using namespace polyexec;
using namespace azmatest;

TEST_SUITE("TypedTask") {
    TEST_CASE("Task<T> wraps result") {
        auto task = make_typed_process_task("echo", {"world"});
        
        std::thread t([&task]() {
            task.base_->execute({});
        });
        
        auto result = task.get();
        t.join();
        
        ASSERT_EQ(result.exit_code, 0);
        ASSERT_TRUE(result.stdout_data.find("world") != std::string::npos);
    }
    
    TEST_CASE("then chains transformations") {
        auto task = make_typed_process_task("echo", {"hello"});
        
        auto upper = task.then([](ProcessResult pr) -> std::string {
            std::string s = pr.stdout_data;
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        });
        
        std::thread t([&task]() {
            task.base_->execute({});
        });
        
        auto result = upper.get();
        t.join();
        
        ASSERT_TRUE(result.find("HELLO") != std::string::npos);
    }
    
    TEST_CASE("try_get returns nullopt when not ready") {
        auto task = make_typed_process_task("sleep", {"1"});
        
        auto result = task.try_get();
        ASSERT_FALSE(result.has_value());
    }
    
    TEST_CASE("try_get returns value when ready") {
        auto task = make_typed_process_task("echo", {"test"});
        
        std::thread t([&task]() {
            task.base_->execute({});
        });
        
        task.get(); // Wait for completion
        t.join();
        
        auto result = task.try_get();
        ASSERT_TRUE(result.has_value());
    }
}

int main() {
    return azmatest::run_all_tests();
}
