#include "../Polyexec.hpp"
#include "../AzmaTest/AzmaTest.hpp"

using namespace polyexec;
using namespace azmatest;

TEST_SUITE("FuturePromise") {
    TEST_CASE("promise fulfills future") {
        Promise<int> promise;
        auto future = promise.get_future();
        
        std::thread t([&promise]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            promise.set_value(42);
        });
        
        int value = future.get();
        t.join();
        
        ASSERT_EQ(value, 42);
    }
    
    TEST_CASE("future is_ready checks readiness") {
        Promise<int> promise;
        auto future = promise.get_future();
        
        ASSERT_FALSE(future.is_ready());
        
        promise.set_value(100);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_TRUE(future.is_ready());
    }
    
    TEST_CASE("future then chains continuations") {
        Promise<int> promise;
        auto future = promise.get_future();
        
        auto doubled = future.then([](int x) { return x * 2; });
        
        std::thread t([&promise]() {
            promise.set_value(21);
        });
        
        int result = doubled.get();
        t.join();
        
        ASSERT_EQ(result, 42);
    }
    
    TEST_CASE("promise set_exception propagates error") {
        Promise<int> promise;
        auto future = promise.get_future();
        
        std::thread t([&promise]() {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("test error")
            ));
        });
        
        bool caught = false;
        try {
            future.get();
        } catch (const std::runtime_error& e) {
            caught = true;
            ASSERT_EQ(std::string(e.what()), "test error");
        }
        
        t.join();
        ASSERT_TRUE(caught);
    }
}

int main() {
    return azmatest::run_all_tests();
}
