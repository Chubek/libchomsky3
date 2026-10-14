Chapter 10 - YAML XML S-Expr BSON MessagePack
=============================================

Subjects
--------
- YAML basic indentation parser profile
- XML tree mapping strategy
- S-Expr tokenization and list model
- MessagePack STKJ-prefixed JSON carrier
- BSON STKJ-prefixed JSON carrier
- Binary raw fallback into Value::Binary
- Adapter-guided cross-format movement
- Querying non-JSON formats through Document
- Known ambiguities and lossy edges
- Practical interoperability recipes

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
