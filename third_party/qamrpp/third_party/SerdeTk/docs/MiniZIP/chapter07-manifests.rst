Chapter 7 - Manifest System
===========================

Subjects
--------
- IManifest contract and ownership model
- archive_path and manifest placement rules
- consume_command command-lifecycle expectations
- serialize output contract
- Built-in INIManifest command grammar
- create_manifest and add_to_manifest workflow
- Manifest entry injection timing
- Replacing existing manifest paths during build
- Designing custom manifest implementations
- Validation and failure reporting patterns

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto zipper = minizip::api::zipper::make_zipper();
       zipper.create_manifest<minizip::api::INIManifest>();
       zipper.add_to_manifest(".section package");
       zipper.add_to_manifest(".kv name MiniZIP");
       zipper.add_to_manifest(".kv version 1.0.0");
       zipper.build_file("release.mz");
       return 0;
   }
