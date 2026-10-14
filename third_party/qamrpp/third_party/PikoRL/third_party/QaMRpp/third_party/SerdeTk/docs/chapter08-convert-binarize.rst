Chapter 8 - Conversion and Binarization
=======================================

Subjects
--------
- Convert<Adapter> flow and intent
- Binarize<Adapter> flow and stricter policy
- conversion::Policy modes
- conversion::Report and loss reporting
- LossKind taxonomy
- Adapter contract: to_common/from_common
- JSONAdapter placeholders for YAML/XML/BSON/MessagePack
- Best-effort vs loss-aware behavior
- Designing custom adapters
- Audit logging of lossy transforms

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
