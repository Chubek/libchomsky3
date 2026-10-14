Chapter 10 - Errors and Verification
====================================

Subjects
--------
- result<T> and result<void> usage conventions
- Message-oriented diagnostics
- Cancellation behavior through hooks
- Archive verification guarantees
- Backend error boundaries for unsupported zstd features
- Manifest command rejection behavior
- Extraction conflict and filesystem failures
- Journal parse and validation diagnostics
- Non-exceptional control flow patterns
- When exceptions still occur inside misuse paths

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"
   #include <iostream>

   int main() {
       auto opened = minizip::api::extractor::open("payload.mz");
       if (!opened.ok()) {
           std::cerr << opened.message() << '\n';
           return 1;
       }
       auto verify = opened.value().verify();
       if (!verify.ok()) {
           std::cerr << verify.message() << '\n';
           return 1;
       }
       return 0;
   }
