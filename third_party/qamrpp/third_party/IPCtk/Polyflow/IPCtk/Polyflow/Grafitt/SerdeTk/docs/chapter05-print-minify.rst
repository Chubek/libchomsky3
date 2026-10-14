Chapter 5 - Printing and Minify
===============================

Subjects
--------
- SimplePrinter behavior
- PrettyPrinter behavior
- Minify facade and helper function
- Formatting options: indent and compact flags
- Format-specific printer selection
- Printing binary-backed documents
- Idempotence considerations
- Newline and whitespace control
- Emission differences across JSON/YAML/XML/S-Expr
- CLI and file-output integration patterns

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
