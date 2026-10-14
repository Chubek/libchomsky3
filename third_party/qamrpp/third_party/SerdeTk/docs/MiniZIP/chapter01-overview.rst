Chapter 1 - Architecture and Setup
==================================

Subjects
--------
- Project scope and header-only delivery
- Primary namespaces: backend, api, dsl, detail
- MiniZIP archive container vs raw compression backend
- Zstandard-oriented backend subset and honesty policy
- Archive pipeline: source -> filters -> compression -> transport
- Public entry points: zipper, extractor, journal_archiver
- DSLtk integration model and thin DSL philosophy
- Header inclusion and zero-build-step consumption
- Filesystem and in-memory workflows
- Supported vs deferred backend features

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto zipper = minizip::api::zipper::make_zipper();
       zipper.add_bytes("hello.txt", minizip::detail::to_bytes("hello"));
       zipper.build_file("hello.mz");
       return 0;
   }
