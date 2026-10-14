Chapter 7 - JSON Schema Validation
==================================

Subjects
--------
- Validator construction from schema text
- Validator construction from schema file
- ValidationReport structure
- Supported keywords: type/properties/required/enum/items
- Path-qualified diagnostics
- validate(doc) boolean/report modes
- validate(doc, ostream) reporting mode
- Schema-format loading via json::format
- Current scope limits and extension points
- Cross-format validation via common model

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
