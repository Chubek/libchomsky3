Chapter 11 - SKTL and ITKD Syntax
=================================

Subjects
--------
- SKTL document structure
- Required sections and semantic checks
- Section braces and nesting rules
- Key-value assignment style
- Supported built-in targets (JSON/YAML/XML/S-Expr/MessagePack/BSON)
- Compile pipeline: parse -> semantic -> compile
- Registry loading from std/*.sktl
- Error diagnostics for malformed SKTL
- ITKD note: this codebase exposes SKTL, not ITKD
- Migration guideline if ITKD is an external alias

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

ITKD/SKTL Syntax Snapshot
-------------------------
.. code-block:: text

   # SerdeTk currently implements SKTL descriptors.
   format JSON {
     category: textual
     extension: .json
   }

   # If your environment uses the term ITKD, map it to the same descriptor grammar.
