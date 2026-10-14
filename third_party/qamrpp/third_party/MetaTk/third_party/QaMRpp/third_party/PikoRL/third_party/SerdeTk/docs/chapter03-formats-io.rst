Chapter 3 - Format and I/O APIs
===============================

Subjects
--------
- CompiledFormat function hooks
- load_file/load_string/load_bytes contracts
- dump_file/dump_string/dump_bytes contracts
- formatting::Options and pretty output
- Namespace facades: json/yaml/xml/sexpr/msgpack/bson
- load_from_file and from_file aliases
- dump_to_file convenience
- Binary fallback behavior for BSON/MessagePack
- Failure modes for unsupported operations
- Choosing file extensions and source_format tagging

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
