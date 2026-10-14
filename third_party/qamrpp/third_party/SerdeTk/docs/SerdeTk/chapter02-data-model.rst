Chapter 2 - Data Model
======================

Subjects
--------
- Value variant layout and scalar types
- Object semantics and key ownership
- Array semantics and insertion order
- Binary carrier semantics
- Document metadata and source_format
- Type inspection: is_null/is_object/...
- Accessors: as_object/as_array/as_binary
- Construction from C++ primitives
- Mutation patterns and aliasing behavior
- Round-trip expectations across formats

Example
-------
.. code-block:: cpp

   #include "SerdeTk.hpp"

   int main() {
       auto doc = serdetk::json::from_string("{"user":{"name":"Ada"}}
");
       auto name = serdetk::query::API(doc).at("user").at("name").value().as_string();
       (void)name;
       return 0;
   }
