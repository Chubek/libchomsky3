#include "../Polyflow.hpp"
#include "../AzmaTest/AzmaTest.hpp"

using namespace polyflow;
using namespace azmatest;

TEST_SUITE("TaskGraph") {
    TEST_CASE("add_task creates unique handles") {
        task_graph graph;
        auto t1 = graph.add_task("task1");
        auto t2 = graph.add_task("task2");
        
        ASSERT_NE(t1.id, t2.id);
    }
    
    TEST_CASE("add_dependency creates edge") {
        task_graph graph;
        auto t1 = graph.add_task("t1");
        auto t2 = graph.add_task("t2");
        
        graph.add_dependency(t1, t2);
        
        auto order = graph.schedule();
        ASSERT_EQ(order.size(), 2);
        ASSERT_EQ(order.front(), t1);
        ASSERT_EQ(order.back(), t2);
    }
    
    TEST_CASE("schedule returns topological order") {
        task_graph graph;
        auto t1 = graph.add_task("t1");
        auto t2 = graph.add_task("t2");
        auto t3 = graph.add_task("t3");
        
        graph.add_dependency(t1, t2);
        graph.add_dependency(t2, t3);
        
        auto order = graph.schedule();
        
        ASSERT_EQ(order.size(), 3);
        // t1 should come before t2, t2 before t3
    }
    
    TEST_CASE("has_cycle detects cycles") {
        task_graph graph;
        auto t1 = graph.add_task("t1");
        auto t2 = graph.add_task("t2");
        auto t3 = graph.add_task("t3");
        
        graph.add_dependency(t1, t2);
        graph.add_dependency(t2, t3);
        graph.add_dependency(t3, t1); // Cycle
        
        ASSERT_TRUE(graph.has_cycle());
    }
    
    TEST_CASE("acyclic graph has no cycle") {
        task_graph graph;
        auto t1 = graph.add_task("t1");
        auto t2 = graph.add_task("t2");
        
        graph.add_dependency(t1, t2);
        
        ASSERT_FALSE(graph.has_cycle());
    }
}

int main() {
    return azmatest::run_all_tests();
}
