Chapter 4 - Creator and Editor Workflows
========================================

Subjects
--------
- Format-bound Creator defaults
- SectionBuilder for object section insertion
- ObjectBuilder set/object/array methods
- ArrayBuilder push semantics
- Schema attachment via set_schema
- Editor::open and in-memory mutation
- Metadata editing and source annotations
- dump() persistence flow
- Memory policy template parameter
- Defensive patterns for missing root object

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
