Chapter 6 - Query Engines
=========================

Subjects
--------
- query::API typed navigation model
- Path tokenization constraints
- Result container semantics
- json::Query<Engine> facade pattern
- Built-in engines: JQ placeholder
- Built-in engines: STKQ subset parser
- Built-in engines: SPARQL placeholder
- Error handling for invalid path segments
- Engine extensibility contract
- Testing query behavior deterministically

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
