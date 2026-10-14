Chapter 1 - Architecture and Setup
==================================

Subjects
--------
- Project scope and header-only delivery
- Core types: Value, Object, Array, Binary, Document
- Format abstraction via CompiledFormat
- Built-in registry and format discovery
- Textual vs binary category model
- File/string/bytes load and dump paths
- Error model: Error, ParseError, FormatError, QueryError
- Minimal build integration with CMake
- Thread-safety model (format singletons)
- Interoperability boundary with DSLUtils and SKTL

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
