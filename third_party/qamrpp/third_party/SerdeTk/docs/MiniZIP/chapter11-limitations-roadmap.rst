Chapter 11 - Limitations and Roadmap
====================================

Subjects
--------
- Implemented Zstd subset boundaries
- Unsupported compressed-block decoding
- Unsupported entropy coding and dictionaries
- Checksum emission and validation gaps
- Middleware registration scope limitations
- MZJL feature gaps and current exclusions
- Archive format versioning considerations
- Performance tradeoffs of eager extraction
- Forward-compatible extension points in backend and API
- Recommended roadmap for fuller Zstd support

Example
-------
.. code-block:: text

   Current backend support:
   - stored passthrough
   - zstd frame emission with Raw_Block blocks
   - zstd frame parsing with Raw_Block and RLE_Block decode
   - compressed zstd blocks rejected explicitly
