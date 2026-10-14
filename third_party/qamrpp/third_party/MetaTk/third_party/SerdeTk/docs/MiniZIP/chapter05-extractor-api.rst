Chapter 5 - Extractor API
=========================

Subjects
--------
- extractor::open from file and buffer transport
- Eager archive decode model
- list_entries and archive browsing
- verify semantics and current scope
- extract_all_to and destination management
- Conflict handling with overwrite policy
- extract_bytes for direct byte recovery
- Typed extraction through deserializers
- Hook integration during extraction
- Directory recreation behavior

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto opened = minizip::api::extractor::open("payload.mz");
       if (!opened.ok()) {
           return 1;
       }
       auto extractor = std::move(opened.value());
       extractor.extract_all_to("out");
       return 0;
   }
