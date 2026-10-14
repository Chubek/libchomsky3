Chapter 2 - Archive Data Model
==============================

Subjects
--------
- Entry metadata fields and invariants
- Entry kinds: file, directory, generated, manifest, object
- Source kinds and payload provenance
- Logical path normalization rules
- Directory identity and trailing slash semantics
- Stored size vs original size accounting
- Payload hashing and verification semantics
- Serializer and filter tags in metadata
- Archive container record layout
- Collision-safe entry replacement and removal

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto zipper = minizip::api::zipper::make_zipper();
       zipper.add_file("README.md");
       zipper.add_directory("assets/", minizip::api::options::Recursive);
       zipper.remove_item("README.md");
       zipper.build_file("bundle.mz");
       return 0;
   }
