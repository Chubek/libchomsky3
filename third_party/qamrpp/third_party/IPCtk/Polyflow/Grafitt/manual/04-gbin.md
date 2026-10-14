# GBIN and Schema Lookup

- Schema root lookup checks `GRAFITT_SCHEMA_DIR` first.
- Fallback schema root is `specs/`.
- `gbin::serialize()` and `gbin::deserialize()` now implement GBIN v1 with deterministic little-endian encoding:
  - magic bytes: `GBIN`
  - version byte: `1`
  - directed flag byte
  - vertex count (`u32`)
  - edge count (`u32`)
  - vertices as length-prefixed UTF-8 strings
  - edges as `(src, dst, label)` length-prefixed strings
- `deserialize` currently targets `std::string` vertices and `std::string`/`unit` edge labels.
