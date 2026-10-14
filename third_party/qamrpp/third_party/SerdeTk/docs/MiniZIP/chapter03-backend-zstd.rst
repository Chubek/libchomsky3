Chapter 3 - Zstandard Backend
=============================

Subjects
--------
- backend::codec selection and stored passthrough mode
- zstd_engine frame emission strategy
- Supported frame parsing elements
- Supported block kinds: Raw_Block and RLE_Block
- Skippable frame handling
- Frame inspection via inspect_frame
- Frame header descriptor interpretation
- Window size, dictionary id, and content size parsing
- Honest rejection of compressed blocks and entropy coding
- Current checksum and dictionary limitations

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto compressed = minizip::backend::zstd_engine::compress(
           minizip::detail::to_bytes("payload"),
           {.algorithm = minizip::backend::codec::zstd}
       );
       auto info = minizip::backend::zstd_engine::inspect_frame(compressed.bytes);
       (void)info;
       return 0;
   }
