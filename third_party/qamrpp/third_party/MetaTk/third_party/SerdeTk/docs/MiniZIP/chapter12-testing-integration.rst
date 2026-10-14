Chapter 12 - Testing and Integration
====================================

Subjects
--------
- Catch2 coverage for backend and archive workflows
- Direct compile validation for header-only delivery
- Testing manifest round-trips
- Testing journal scaffolding flows
- Testing DSL-to-API equivalence
- Suggested future regression tests
- Build-system integration expectations
- Include path requirements for DSLtk.hpp
- Packaging considerations for installed headers
- Documentation maintenance strategy

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto zipper = minizip::api::zipper::make_zipper();
       auto bytes = zipper.build_archive_bytes();
       return bytes.ok() ? 0 : 1;
   }
