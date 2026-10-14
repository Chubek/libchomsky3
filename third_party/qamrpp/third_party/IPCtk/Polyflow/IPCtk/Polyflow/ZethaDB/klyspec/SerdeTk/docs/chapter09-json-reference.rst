Chapter 9 - JSON Reference Implementation
=========================================

Subjects
--------
- JSON parser scope and grammar subset
- Number handling and integer/float branching
- String decoding behavior
- Object/array recursion model
- Emitter escaping rules
- Pretty vs compact output
- Round-trip caveats for number width
- json::minify and dump_to_file usage
- Interfacing with Validator and Query
- Performance notes for large documents

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
