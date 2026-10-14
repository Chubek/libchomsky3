Chapter 9 - Native C++ DSL
==========================

Subjects
--------
- dsl namespace purpose and thin-wrapper philosophy
- Real DSLtk pipeline helper usage
- archive and extract starting expressions
- from, level, save_to, to, on_conflict stages
- filter_with and hook_with stages
- serialize_with and deserialize_with stages
- manifest and manifest_cmd stages
- Mapping DSL expressions onto api::zipper and api::extractor
- Zero-overhead staging expectations
- When to prefer API builders over the DSL

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto plan =
           minizip::dsl::archive("assets.mz")
           | minizip::dsl::from("assets/", minizip::api::options::Recursive)
           | minizip::dsl::level(8)
           | minizip::dsl::save_to("dist");

       auto result = plan.build();
       return result.ok() ? 0 : 1;
   }
