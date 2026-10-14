Chapter 12 - Extensibility and Testing
======================================

Subjects
--------
- Adding a new format namespace facade
- Defining a new CompiledFormat backend
- Registering formats in FormatRegistry
- Implementing custom Query engines
- Implementing custom Adapters
- Validation extension architecture
- Core unit-test matrix
- Round-trip and fuzz test strategy
- Backward compatibility constraints
- Roadmap alignment with SerdeTk phases

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
