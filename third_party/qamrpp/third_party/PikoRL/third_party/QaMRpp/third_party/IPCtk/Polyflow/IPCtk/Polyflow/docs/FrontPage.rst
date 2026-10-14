Polyflow Documentation
======================

**Polyflow** is a modern C++20 header-only library for graph-based task concurrency.

Overview
--------

Polyflow provides:

- **Task Graph Orchestration**: Define task dependencies as a directed acyclic graph (DAG)
- **Work-Stealing Scheduler**: Efficient multi-threaded execution with LIFO local / FIFO steal semantics
- **Process Integration**: Execute external processes as tasks with resource limits and retry policies
- **Type-Safe I/O**: Strongly-typed task inputs and outputs using ZethaMEM values
- **Real-Time Monitoring**: Standalone TUI monitor for observing live taskflow execution
- **Cancellation & Timeouts**: Cooperative cancellation and timeout enforcement
- **Continuation Chaining**: Compose tasks with `.then()` for functional-style pipelines

Architecture
------------

Polyflow integrates several specialized libraries:

- **Grafitt**: Graph storage and topological algorithms
- **ZethaMEM**: Typed value model for task I/O
- **IPCtk**: Inter-process communication for monitoring
- **DSLUtils**: Fluent DSL composition primitives

Core Components
---------------

**Polyflow.hpp**
  Main orchestration layer providing task graph, scheduler, and execution context.

**Polyexec.hpp**
  Execution layer with process task factory, typed task wrappers, and retry policies.

**polyflow-monitor**
  Standalone TUI binary for real-time taskflow monitoring via IPC.

Quick Start
-----------

.. code-block:: cpp

   #include "Polyflow.hpp"
   #include "Polyexec.hpp"
   
   int main() {
       using namespace polyflow;
       
       // Create task graph
       task_graph graph;
       auto t1 = graph.add_task("load_data");
       auto t2 = graph.add_task("process");
       auto t3 = graph.add_task("save");
       
       graph.add_dependency(t1, t2);
       graph.add_dependency(t2, t3);
       
       // Schedule and execute
       execution_context ctx(8); // 8 worker threads
       ctx.run_all(graph);
       
       return 0;
   }

Documentation Sections
----------------------

- :doc:`manual/01_introduction`
- :doc:`manual/02_architecture`
- :doc:`manual/03_api_reference`
- :doc:`manual/04_examples`
- :doc:`manual/05_advanced_topics`

License
-------

See LICENSE file for details.
