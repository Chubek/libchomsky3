# COMPLETING_STORAGE.md
## Completing `ZethaSTORAGE.hpp` (ZethaDB / ZethaREL storage backend)

This document is written for GPT Codex (and humans) to finish implementing `ZethaSTORAGE.hpp`: a **header-only**, **page-based B+Tree** storage layer that underpins `ZethaREL.hpp`, supports journaling-based fast rebuild, defragmentation, rebalancing, mmap I/O, schema parsing via `DSLUtils.hpp`, and an embedded Python command stub.

**Non-goals (for now):**
- No political content or commentary.
- No full SQL engine; query evaluation belongs to `ZethaQUERY.hpp` and `ZethaREL.hpp`.
- No multi-file CMake/project plumbing; keep it header-only.

---

## 0) Guiding principles

1. **Header-only, C++20**, portable first (Linux/macOS), Windows can be stubbed behind `#if _WIN32`.
2. **On-disk structs are POD**: `#pragma pack(push, 1)` + `static_assert(std::is_trivially_copyable_v<...>)`.
3. **Runtime structs separate from format structs**: do not put `std::string`, `std::vector`, vtables, or pointers in packed structs.
4. **All mmap windows are page-aligned and page-sized**: offsets and lengths must be multiples of `page_size`.
5. **Journal drives recovery and rebuilding**: the page map (what’s in which page) must be derivable quickly from the journal.
6. **Schema stored in header as JSON Schema base64** using `zbase64.hpp` (`zbase64::encode/decode`).
7. **Breadcumbs always captured** during traversal for fast updates (splits/merges) and for debug tooling.
8. **Defragmentation is a first-class operation**: pages move; page map + journal can redirect/rewrite.

---

## 1) Deliverables inside `ZethaSTORAGE.hpp`

Codex must implement these high-level modules (all inside one header file, namespaced):

### 1.1 File format & rich header
- Shebang at file start to allow `./db.zdb <command>` execution.
- Rich header includes:
  - user-selected magic number (bytes)
  - page size, block size
  - feature flags (crc, encryption marker, compression marker, etc.)
  - offsets for:
    - schema (base64 JSON Schema)
    - journal region
    - page-map region (optional, can be rebuilt from journal)
    - embedded Python script region (`python_begin`, `python_end`)
- Header must be forward-compatible:
  - `header_version`
  - reserved fields
  - capability flags

**Schema storage**:
- `schema_json` (UTF-8) → base64 via `zbase64::encode`.
- Store as contiguous bytes in its own region; offsets recorded in header.

### 1.2 Page system (B+Tree pages)
- Fixed `page_size` chosen at creation time.
- Each page begins with `PageHeader`:
  - `page_id`
  - `kind` (internal/leaf/freelist/journal/pagemap)
  - `level`
  - `cell_count`
  - `free_bytes`
  - `right_sibling` for leaf chaining
  - `lsn` (last journal sequence number applied to page)

**Page content design** (recommended):
- Use a **slotted page** layout to support variable-length cells:
  - header at front
  - cell data grows from end backwards
  - slot array grows from header forward
- This enables deletions/updates without immediate compaction.

### 1.3 B+Tree operations
Implement:
- `get(key)`
- `insert(key, value)`
- `erase(key)`
- leaf chaining for range scans later (ZethaREL/ZethaQUERY)
- split/merge/rebalance operations logged in journal
- breadcrumb collection: traversal returns `Breadcrumbs` describing path.

**Key ordering**:
- Decide and document: keys are byte-ordered lexicographically unless schema provides typed collation.
- Keep a `KeyCodec` layer for future typed keys.

### 1.4 Cache + index
- Page cache (LRU-ish) holds recently used pages.
- Dirty tracking.
- Fast index:
  - in-memory page map derived from journal (page_id → range or hash metadata)
  - optional persisted page map region (snapshot) to accelerate open.

### 1.5 Journal (append-only)
Implement:
- `JournalEntryHeader { kind, bytes, lsn, unix_ns }`
- payloads for:
  - Insert
  - Erase
  - PageAlloc / PageFree
  - Split / Merge / Rebalance
  - DefragMove (page relocation)
  - PageMapUpdate (optional)

Properties:
- Always append-only.
- Journal is the source of truth for:
  - rebuild tree
  - rebuild page map
  - replay after crash (at least best-effort)

**Rebuild**:
- `rebuild_from_journal()`:
  - reads journal sequentially
  - reconstructs pages/tree
  - writes fresh compacted structure
  - optionally produces a new journal snapshot boundary.

### 1.6 Defragmentation
- `defragment()`:
  - moves pages to reduce holes and improve locality.
  - updates page references and leaf sibling links.
  - records moves in journal so page map can be updated / replayed.

### 1.7 Bulk loading
- `bulk_load(sorted_entries)`:
  - build B+Tree bottom-up:
    - fill leaf pages sequentially
    - create parent internal pages
  - journal records a `BulkLoadBegin`, `BulkLoadCommit` (new kinds)
  - produces minimal fragmentation.

### 1.8 Memory mapping
- Implement `MMapWindow`:
  - map/unmap page-aligned windows
  - allow mapping:
    - header + schema region
    - a run of pages
    - journal tail
- Provide safe read/write wrappers; keep `munmap` always called.

### 1.9 Embedded Python script
- At end of `.zdb`, embed a Python script stub.
- Header must record `python_begin/python_end`.
- Script should parse CLI commands:
  - `print-header --json --sink=$stdout`
  - `defragment`
  - `rebuild --journal=...`
  - `compress --zip --out=...`
  - `query --file=myquery.zdq`
  - `cron ...`
  - `clean`
  - `encrypt --password`
  - plus: `validate`, `dump-schema`, `stats`, `journal-tail`, `page-map`
- For now script can be minimal but must remain valid Python.

---

## 2) Integration requirements with existing headers

### 2.1 Base64: `zbase64.hpp`
Use:
- `zbase64::encode(std::string_view input, bool urlsafe=false)`
- `zbase64::decode(std::string_view input, bool urlsafe=false)`

**Rule**: schema stored as JSON Schema base64, default `urlsafe=false`.

### 2.2 Schema language: `DSLUtils.hpp`
ZethaSTORAGE schema must have:
- **textual DSL** (parsed via DSLUtils combinators)
- **native C++ DSL** (builders)
- both compile down to a JSON Schema string saved in header.

Codex TODOs:
- locate/understand DSLUtils parser APIs:
  - parser type
  - sequence/alt combinators
  - `Result` / `Maybe`
  - failure reporting (`ParseFailureKind`)
- implement `parse_schema_text(...) -> Expected<Schema>`
- implement C++ builder DSL: `schema().table("...").col("...", type::i64)....`

**Important**: keep schema parsing independent of storage operations.

### 2.3 ZethaREL / ZethaQUERY hook points
- Provide stable storage APIs usable by ZethaREL:
  - open/create/close
  - get/insert/erase
  - scan/range iterators (may be stubbed but design now)
  - transactions (optional placeholder)
- Provide metadata export for query engine:
  - schema json
  - stats (page counts, tree height, journal size)

---

## 3) Concrete on-disk layout (recommended)

Codex should standardize this layout:
