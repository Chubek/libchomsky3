#pragma once
// ZethaSTORAGE.hpp - header-only page-based B+Tree storage engine for ZethaREL
// --------------------------------------------------------------------------
// Codex hint:
// - Keep everything header-only and in namespaces.
// - Prefer POD structs for on-disk layouts; validate with static_assert(sizeof==...).
// - Separate "format structs" (packed) from "runtime structs" (safe, owning).
// - All offsets/lengths in header are little-endian unless specified.
// - Never mmap non-page-aligned windows; always align to page_size.
//
// NOTE: This is a scaffold. Schema DSL parsing via DSLUtils is stubbed.
//       Once the DSLUtils parser API is available, we will implement it.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <map>
#include <set>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <chrono>
#include <algorithm>
#include <utility>
#include <limits>
#include <fstream>
#include <sstream>
#include <array>
#include <filesystem>
#include <functional>

#include "zbase64.hpp" // requires zbase64::encode/decode

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

namespace zetha::storage {

// -----------------------------
// Error model
// -----------------------------
enum class Errc {
  Ok = 0,
  Io,
  Corrupt,
  InvalidArgument,
  Unsupported,
  NotFound,
  Busy,
  OutOfMemory,
  ParseError,
  JournalError,
  SchemaMismatch,
};

struct Error {
  Errc code{Errc::Ok};
  std::string message;
};

template <class T>
struct ExpectedType {
  using type = std::variant<T, Error>;
};
template <>
struct ExpectedType<void> {
  using type = std::optional<Error>;
};
template <class T>
using Expected = typename ExpectedType<T>::type;

inline bool ok(const Error& e) { return e.code == Errc::Ok; }

// -----------------------------
// Format constants
// -----------------------------
struct FormatLimits {
  static constexpr std::uint32_t kHeaderMaxBytes = 64 * 1024; // rich header cap
  static constexpr std::uint32_t kMinPageSize = 4096;
  static constexpr std::uint32_t kMaxPageSize = 4 * 1024 * 1024;
  static constexpr std::uint32_t kMaxMagicBytes = 32;
};

// -----------------------------
// Binary header (on-disk)
// -----------------------------
// Codex hint: Keep this trivially-copyable; avoid std::string here.
#pragma pack(push, 1)
struct FileHeaderFixed {
  // Shebang region precedes this header on disk; header_offset points here.
  std::uint32_t header_version = 1;

  // User-selectable magic (stored separately as bytes in variable header region)
  std::uint32_t magic_len = 0;
  std::uint64_t created_unix_ns = 0;
  std::uint64_t last_open_unix_ns = 0;

  // Page/block geometry
  std::uint32_t page_size = 0;   // must be multiple of block_size
  std::uint32_t block_size = 0;  // user-defined; written here
  std::uint32_t endian = 0x01020304; // sanity (we store LE; used as check)
  std::uint32_t flags = 0;       // feature flags

  // Root of B+Tree
  std::uint64_t root_page_id = 0;
  std::uint64_t freelist_page_id = 0;

  // Journal region (append-only)
  std::uint64_t journal_begin = 0; // file offset
  std::uint64_t journal_end = 0;   // file offset (append position)

  // Page map (journal-derived quick index)
  std::uint64_t page_map_begin = 0;
  std::uint64_t page_map_end = 0;

  // Schema: JSON Schema in base64
  std::uint64_t schema_b64_begin = 0;
  std::uint64_t schema_b64_end = 0;

  // Embedded python helper script offset (within file)
  std::uint64_t python_begin = 0;
  std::uint64_t python_end = 0;

  // Reserved for future (encryption, compression, checksums)
  std::uint64_t reserved0 = 0;
  std::uint64_t reserved1 = 0;
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable_v<FileHeaderFixed>);

// Variable header region layout (immediately after FileHeaderFixed in file):
// [magic bytes][header_json_utf8? optional][... future fields ...]
// In this scaffold we only store magic bytes; schema is stored in schema_b64 region.

// -----------------------------
// Page types and on-disk layout
// -----------------------------
enum class PageKind : std::uint16_t {
  Invalid = 0,
  BPlus_Internal = 1,
  BPlus_Leaf = 2,
  FreeList = 3,
  Journal = 4,
  PageMap = 5,
};

#pragma pack(push, 1)
struct PageHeader {
  std::uint64_t page_id = 0;
  std::uint16_t kind = 0;     // PageKind
  std::uint16_t level = 0;    // B+Tree level (0=leaf)
  std::uint32_t crc32 = 0;    // optional; 0 means not used in scaffold
  std::uint64_t lsn = 0;      // log sequence number (journal position)
  std::uint16_t cell_count = 0;
  std::uint16_t reserved16 = 0;
  std::uint32_t free_bytes = 0;
  std::uint64_t right_sibling = 0; // for leaf linking
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable_v<PageHeader>);

// Cells are variable-sized records inside the page.
// For now: [key_len u32][val_len u32][key bytes][val bytes]
// Internal page "val" becomes child_page_id (u64) encoded as bytes.

// -----------------------------
// Breadcrumb traversal
// -----------------------------
struct Breadcrumb {
  std::uint64_t page_id = 0;
  std::uint32_t child_index = 0; // which pointer we followed
  std::uint16_t level = 0;
};

using Breadcrumbs = std::vector<Breadcrumb>;

// -----------------------------
// Journal entries (append-only log)
// -----------------------------
enum class JournalKind : std::uint16_t {
  Invalid = 0,
  Insert = 1,
  Erase = 2,
  Split = 3,
  Merge = 4,
  Rebalance = 5,
  PageAlloc = 6,
  PageFree = 7,
  PageMapUpdate = 8,
  DefragMove = 9,
};

#pragma pack(push, 1)
struct JournalHeader {
  std::uint16_t kind = 0; // JournalKind
  std::uint16_t reserved = 0;
  std::uint32_t bytes = 0; // total entry bytes including header
  std::uint64_t lsn = 0;
  std::uint64_t unix_ns = 0;
};
#pragma pack(pop)

enum class KvJournalOp : std::uint8_t {
  Put = 1,
  Erase = 2,
  TxnBegin = 16,
  TxnEnd = 17,
  Defragment = 18,
};

#pragma pack(push, 1)
struct KvJournalRecordHeader {
  std::uint8_t op = 0;
  std::uint8_t reserved = 0;
  std::uint16_t flags = 0;
  std::uint32_t key_len = 0;
  std::uint32_t value_len = 0;
  std::uint64_t lsn = 0;
  std::uint64_t txn_id = 0;
};
#pragma pack(pop)

// Codex hint: journal payloads should be versioned and self-describing.
// For now, keep it simple and extensible via tagged payload.

// -----------------------------
// Cache and page map index
// -----------------------------
struct CacheConfig {
  std::size_t max_pages = 4096;
  bool track_dirty = true;
};

struct PageBuffer {
  std::vector<std::byte> bytes;
  bool dirty = false;
  std::uint64_t page_id = 0;
  PageKind kind = PageKind::Invalid;
};

class PageCache {
public:
  explicit PageCache(CacheConfig cfg) : cfg_(cfg) {}

  PageBuffer* get(std::uint64_t page_id) {
    auto it = pages_.find(page_id);
    if (it == pages_.end()) return nullptr;
    touch_(page_id);
    return &it->second;
  }

  PageBuffer& put(PageBuffer buf) {
    if (pages_.size() >= cfg_.max_pages) evict_one_();
    auto id = buf.page_id;
    pages_.emplace(id, std::move(buf));
    touch_(id);
    return pages_.at(id);
  }

  void mark_dirty(std::uint64_t page_id) {
    if (auto* p = get(page_id)) p->dirty = true;
  }

  std::vector<std::uint64_t> dirty_pages() const {
    std::vector<std::uint64_t> out;
    out.reserve(pages_.size());
    for (auto const& [id, pb] : pages_) if (pb.dirty) out.push_back(id);
    return out;
  }

private:
  void touch_(std::uint64_t page_id) {
    lru_.erase(page_id);
    lru_.emplace(page_id);
  }

  void evict_one_() {
    if (lru_.empty()) return;
    auto it = lru_.begin();
    auto victim = *it;
    // Codex hint: before eviction, flush if dirty (caller-managed in this scaffold).
    lru_.erase(it);
    pages_.erase(victim);
  }

  CacheConfig cfg_;
  std::unordered_map<std::uint64_t, PageBuffer> pages_;
  // simplistic LRU: ordered set of ids; begin() is "oldest" due to touch_ logic.
  std::set<std::uint64_t> lru_;
};

// Page map: tracks what is stored in which page.
// Scaffold: keep in-memory map; later persist in page_map region and journal it.
struct PageMapEntry {
  std::uint64_t page_id = 0;
  PageKind kind = PageKind::Invalid;
  std::uint64_t min_key_hash = 0;
  std::uint64_t max_key_hash = 0;
};

// -----------------------------
// Memory mapping helper (page-aligned windows)
// -----------------------------
class MMapWindow {
public:
  MMapWindow() = default;
  ~MMapWindow() { unmap(); }

  MMapWindow(const MMapWindow&) = delete;
  MMapWindow& operator=(const MMapWindow&) = delete;

  Expected<void*> map_readwrite(std::string_view path,
                               std::uint64_t offset,
                               std::uint64_t length,
                               std::uint32_t page_size) {
    if (auto v = validate_page_window(offset, length, page_size); v.has_value()) return v.value();
    unmap();

#if defined(_WIN32)
    (void)path; (void)offset; (void)length;
    return Error{Errc::Unsupported, "Windows mmap not implemented in scaffold"};
#else
    fd_ = ::open(std::string(path).c_str(), O_RDWR);
    if (fd_ < 0) return Error{Errc::Io, "open failed"};
    length_ = static_cast<std::size_t>(length);
    offset_ = static_cast<off_t>(offset);
    void* p = ::mmap(nullptr, length_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, offset_);
    if (p == MAP_FAILED) {
      ::close(fd_); fd_ = -1;
      return Error{Errc::Io, "mmap failed"};
    }
    base_ = p;
    return base_;
#endif
  }

  static Expected<void> validate_page_window(std::uint64_t offset,
                                             std::uint64_t length,
                                             std::uint32_t page_size) {
    if (page_size == 0) return Error{Errc::InvalidArgument, "page_size=0"};
    if (offset % page_size != 0) return Error{Errc::InvalidArgument, "offset not page-aligned"};
    if (length % page_size != 0) return Error{Errc::InvalidArgument, "length not page-aligned"};
    return std::nullopt;
  }

  void unmap() {
#if defined(_WIN32)
    base_ = nullptr;
    length_ = 0;
#else
    if (base_ && length_) {
      ::munmap(base_, length_);
    }
    base_ = nullptr;
    length_ = 0;
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
  }

  void* base() const { return base_; }
  std::size_t size() const { return length_; }

private:
  void* base_ = nullptr;
  std::size_t length_ = 0;
#if defined(_WIN32)
  // placeholder
#else
  int fd_ = -1;
  off_t offset_ = 0;
#endif
};

// -----------------------------
// Schema representation (stub)
// -----------------------------
// The header stores JSON Schema (as base64). We will generate it from:
// 1) textual schema DSL parsed by DSLUtils, OR
// 2) native C++ schema DSL (builders).
//
// Scaffold keeps schema as plain JSON string.
struct Schema {
  std::string json_schema; // UTF-8 JSON Schema
};

// TODO (integration): parse schema DSL using DSLUtils combinators.
// Expected<Schema> parse_schema_text(std::string_view);

// -----------------------------
// Storage config & open/create
// -----------------------------
struct CreateOptions {
  std::vector<std::byte> magic; // user-selectable magic
  std::uint32_t page_size = 4096;
  std::uint32_t block_size = 4096;
  Schema schema;
  bool enable_crc = false;
  bool enable_journal = true;
};

struct OpenOptions {
  bool read_only = false;
  bool verify_header = true;
};

class ZethaStorage {
public:
  struct ScanItem {
    std::string_view key;
    std::string_view value;
  };

  class ScanIterator {
  public:
    bool valid() const { return it_ != end_; }
    void next() { if (it_ != end_) ++it_; }
    ScanItem item() const { return ScanItem{it_->first, it_->second}; }

  private:
    using It = std::map<std::string, std::string>::const_iterator;
    ScanIterator(It it, It end) : it_(it), end_(end) {}
    It it_;
    It end_;
    friend class ZethaStorage;
  };

  ZethaStorage() = default;
  ~ZethaStorage() { (void)close(); }

  ZethaStorage(const ZethaStorage&) = delete;
  ZethaStorage& operator=(const ZethaStorage&) = delete;
  ZethaStorage(ZethaStorage&&) = default;
  ZethaStorage& operator=(ZethaStorage&&) = default;

  // -----------------------
  // File lifecycle
  // -----------------------
  static Expected<ZethaStorage> create(std::string path, const CreateOptions& opt) {
    if (opt.page_size < FormatLimits::kMinPageSize || opt.page_size > FormatLimits::kMaxPageSize)
      return Error{Errc::InvalidArgument, "invalid page_size"};
    if (opt.block_size == 0 || (opt.page_size % opt.block_size) != 0)
      return Error{Errc::InvalidArgument, "page_size must be multiple of block_size"};
    if (opt.magic.size() > FormatLimits::kMaxMagicBytes)
      return Error{Errc::InvalidArgument, "magic too long"};

    ZethaStorage db;
    db.path_ = std::move(path);
    db.page_size_ = opt.page_size;
    db.block_size_ = opt.block_size;
    db.schema_ = opt.schema;

    // 1) Lay down shebang + header region + initial pages.
    // Codex hint: We keep this simple: write a shebang, then header, then allocate root leaf page.
    auto now_ns = unix_now_ns_();

    FileHeaderFixed fh{};
    fh.header_version = 1;
    fh.magic_len = static_cast<std::uint32_t>(opt.magic.size());
    fh.created_unix_ns = now_ns;
    fh.last_open_unix_ns = now_ns;
    fh.page_size = opt.page_size;
    fh.block_size = opt.block_size;
    fh.flags = 0;
    if (opt.enable_crc) fh.flags |= 0x1;
    if (opt.enable_journal) fh.flags |= 0x2;

    // Offsets: [shebang + headerfixed + magic] then page 0 begins at next page boundary.
    std::string shebang = "#!/usr/bin/env python3\n";
    // We'll embed a python stub at end; python_begin/python_end set after file creation.

    // Compute schema region content.
    const std::string schema_b64 = zbase64::encode(opt.schema.json_schema);
    // We'll place schema right after header area, before first page, page-aligned.
    // Layout:
    // [shebang][FileHeaderFixed][magic][pad->pagealign][schema_b64][pad->pagealign][pages...][python]
    std::uint64_t off = 0;
    off += static_cast<std::uint64_t>(shebang.size());
    const std::uint64_t header_offset = off;
    off += sizeof(FileHeaderFixed);
    const std::uint64_t magic_begin = off;
    off += opt.magic.size();

    const std::uint64_t pad1 = pad_to_(off, opt.page_size) - off;
    off += pad1;

    fh.schema_b64_begin = off;
    fh.schema_b64_end = off + schema_b64.size();
    off = fh.schema_b64_end;

    const std::uint64_t pad2 = pad_to_(off, opt.page_size) - off;
    off += pad2;

    // First page starts here (page_id=1 for root to keep "0=invalid" convention).
    const std::uint64_t page0_off = off;
    (void)page0_off;

    fh.root_page_id = 1;
    fh.freelist_page_id = 2;

    fh.journal_begin = 0;
    fh.journal_end = 0;

    // Write file
    {
      std::ofstream f(db.path_, std::ios::binary | std::ios::trunc);
      if (!f) return Error{Errc::Io, "cannot create file"};

      // shebang
      f.write(shebang.data(), static_cast<std::streamsize>(shebang.size()));

      // fixed header
      f.write(reinterpret_cast<const char*>(&fh), sizeof(fh));

      // magic bytes
      if (!opt.magic.empty())
        f.write(reinterpret_cast<const char*>(opt.magic.data()),
                static_cast<std::streamsize>(opt.magic.size()));

      // pad to page boundary
      write_zeros_(f, pad1);

      // schema b64
      f.write(schema_b64.data(), static_cast<std::streamsize>(schema_b64.size()));

      // pad
      write_zeros_(f, pad2);

      // allocate initial pages (root leaf + freelist)
      db.write_blank_page_(f, /*page_id*/1, PageKind::BPlus_Leaf, /*level*/0);
      db.write_blank_page_(f, /*page_id*/2, PageKind::FreeList, /*level*/0);

      // embedded python stub
      const auto py = embedded_python_stub_();
      fh.python_begin = static_cast<std::uint64_t>(f.tellp());
      f.write(py.data(), static_cast<std::streamsize>(py.size()));
      fh.python_end = fh.python_begin + py.size();

      // journal region begins after embedded helper; append-only.
      fh.journal_begin = fh.python_end;
      fh.journal_end = fh.journal_begin;

      // rewrite header with correct journal/python offsets
      f.seekp(static_cast<std::streamoff>(header_offset), std::ios::beg);
      f.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    }

    // open runtime
    auto opened = open(db.path_, OpenOptions{});
    if (std::holds_alternative<Error>(opened)) return std::get<Error>(opened);
    return Expected<ZethaStorage>{std::move(std::get<ZethaStorage>(opened))};
  }

  static Expected<ZethaStorage> open(std::string path, const OpenOptions& opt) {
    ZethaStorage db;
    db.path_ = std::move(path);

    // Read shebang first line, then header fixed right after it.
    // Codex hint: Shebang is optional but we assume it exists in this format.
    std::ifstream f(db.path_, std::ios::binary);
    if (!f) return Error{Errc::Io, "cannot open file"};

    std::string first_line;
    std::getline(f, first_line);
    db.has_shebang_ = first_line.rfind("#!", 0) == 0;

    std::streamoff header_off = static_cast<std::streamoff>(first_line.size() + 1); // + newline
    db.header_offset_ = static_cast<std::uint64_t>(header_off);

    FileHeaderFixed fh{};
    f.seekg(header_off, std::ios::beg);
    f.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    if (!f) return Error{Errc::Io, "header read failed"};

    if (opt.verify_header) {
      if (fh.page_size < FormatLimits::kMinPageSize || fh.page_size > FormatLimits::kMaxPageSize)
        return Error{Errc::Corrupt, "bad page_size"};
      if (fh.block_size == 0 || (fh.page_size % fh.block_size) != 0)
        return Error{Errc::Corrupt, "bad block/page geometry"};
    }

    db.fh_ = fh;
    db.page_size_ = fh.page_size;
    db.block_size_ = fh.block_size;

    // Read magic
    std::vector<std::byte> magic(fh.magic_len);
    if (fh.magic_len) {
      f.read(reinterpret_cast<char*>(magic.data()), fh.magic_len);
      if (!f) return Error{Errc::Io, "magic read failed"};
    }
    db.magic_ = std::move(magic);

    // Read schema b64 and decode
    if (fh.schema_b64_end < fh.schema_b64_begin) return Error{Errc::Corrupt, "schema offsets"};
    const auto schema_len = static_cast<std::size_t>(fh.schema_b64_end - fh.schema_b64_begin);
    std::string schema_b64(schema_len, '\0');
    f.seekg(static_cast<std::streamoff>(fh.schema_b64_begin), std::ios::beg);
    f.read(schema_b64.data(), static_cast<std::streamsize>(schema_len));
    if (!f) return Error{Errc::Io, "schema read failed"};
    db.schema_.json_schema = zbase64::decode(schema_b64);

    db.cache_ = std::make_unique<PageCache>(CacheConfig{});
    if (auto loaded = db.load_kv_journal_(); loaded.has_value()) return loaded.value();
    return Expected<ZethaStorage>{std::move(db)};
  }

  Expected<void> close() {
    // Codex hint: flush dirty pages + journal if enabled.
    // Scaffold: no-op.
    return std::nullopt;
  }

  // -----------------------
  // Basic KV operations (scaffold)
  // -----------------------
  Expected<void> insert(std::string_view key, std::string_view value) {
    if (key.empty()) return Error{Errc::InvalidArgument, "empty key"};
    kv_index_[std::string(key)] = std::string(value);
    return append_kv_journal_op_(KvJournalOp::Put, key, value);
  }

  Expected<void> erase(std::string_view key) {
    if (key.empty()) return Error{Errc::InvalidArgument, "empty key"};
    kv_index_.erase(std::string(key));
    return append_kv_journal_op_(KvJournalOp::Erase, key, {});
  }

  Expected<std::optional<std::string>> get(std::string_view key) {
    if (key.empty()) return Error{Errc::InvalidArgument, "empty key"};
    const auto it = kv_index_.find(std::string(key));
    if (it == kv_index_.end()) return std::optional<std::string>{};
    return std::optional<std::string>{it->second};
  }

  ScanIterator scan_begin() const { return ScanIterator{kv_index_.cbegin(), kv_index_.cend()}; }
  ScanIterator scan_end() const { return ScanIterator{kv_index_.cend(), kv_index_.cend()}; }

  Expected<std::vector<std::pair<std::string, std::string>>> scan_snapshot_from_journal() const {
    std::map<std::string, std::string> tmp;
    if (auto r = replay_journal_(tmp); r.has_value()) return r.value();
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(tmp.size());
    for (const auto& kv : tmp) out.emplace_back(kv.first, kv.second);
    return out;
  }

  // -----------------------
  // Maintenance ops (scaffold)
  // -----------------------
  Expected<void> rebuild_from_journal() {
    return load_kv_journal_();
  }

  Expected<void> defragment() {
    std::fstream f(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!f) return Error{Errc::Io, "cannot open file for defragment"};
    std::uint64_t off = fh_.journal_begin;
    const std::uint64_t txn_id = next_txn_id_++;
    auto push = [&](KvJournalOp op, std::string_view k, std::string_view v) -> Expected<void> {
      auto r = write_kv_record_(f, off, op, k, v, txn_id);
      if (std::holds_alternative<Error>(r)) return std::get<Error>(r);
      off = std::get<std::uint64_t>(r);
      return std::nullopt;
    };
    if (auto e = push(KvJournalOp::TxnBegin, {}, {}); e.has_value()) return e;
    for (const auto& kv : kv_index_) {
      if (auto e = push(KvJournalOp::Put, kv.first, kv.second); e.has_value()) return e;
    }
    if (auto e = push(KvJournalOp::Defragment, {}, {}); e.has_value()) return e;
    if (auto e = push(KvJournalOp::TxnEnd, {}, {}); e.has_value()) return e;
    fh_.journal_end = off;
    f.seekp(static_cast<std::streamoff>(header_offset_), std::ios::beg);
    f.write(reinterpret_cast<const char*>(&fh_), sizeof(fh_));
    if (!f) return Error{Errc::Io, "header update failed"};
    std::error_code ec;
    std::filesystem::resize_file(path_, fh_.journal_end, ec);
    if (ec) return Error{Errc::Io, "journal truncate failed"};
    return std::nullopt;
  }

  Expected<void> rebalance() {
    return Error{Errc::Unsupported, "rebalance not implemented in scaffold"};
  }

  // -----------------------
  // Introspection
  // -----------------------
  const Schema& schema() const { return schema_; }
  std::uint32_t page_size() const { return page_size_; }
  std::uint32_t block_size() const { return block_size_; }
  const FileHeaderFixed& header_fixed() const { return fh_; }
  std::string_view path() const { return path_; }

private:
  static std::uint64_t pad_to_(std::uint64_t x, std::uint64_t align) {
    return (x + align - 1) / align * align;
  }

  static void write_zeros_(std::ofstream& f, std::uint64_t n) {
    static constexpr std::size_t kChunk = 4096;
    std::array<char, kChunk> zeros{};
    while (n) {
      auto step = static_cast<std::size_t>(std::min<std::uint64_t>(n, kChunk));
      f.write(zeros.data(), static_cast<std::streamsize>(step));
      n -= step;
    }
  }

  static std::uint64_t unix_now_ns_() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count()
    );
  }

  void write_blank_page_(std::ofstream& f, std::uint64_t page_id, PageKind kind, std::uint16_t level) {
    std::vector<std::byte> buf(page_size_, std::byte{0});
    PageHeader ph{};
    ph.page_id = page_id;
    ph.kind = static_cast<std::uint16_t>(kind);
    ph.level = level;
    ph.cell_count = 0;
    ph.free_bytes = static_cast<std::uint32_t>(page_size_ - sizeof(PageHeader));
    std::memcpy(buf.data(), &ph, sizeof(ph));
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
  }

  Expected<void> append_kv_journal_op_(KvJournalOp op, std::string_view key, std::string_view value) {
    std::fstream f(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!f) return Error{Errc::Io, "cannot open file for journaling"};
    std::uint64_t off = fh_.journal_end;
    const std::uint64_t txn_id = next_txn_id_++;
    auto push = [&](KvJournalOp rec_op, std::string_view k, std::string_view v) -> Expected<void> {
      auto r = write_kv_record_(f, off, rec_op, k, v, txn_id);
      if (std::holds_alternative<Error>(r)) return std::get<Error>(r);
      off = std::get<std::uint64_t>(r);
      return std::nullopt;
    };
    if (auto e = push(KvJournalOp::TxnBegin, {}, {}); e.has_value()) return e;
    if (auto e = push(op, key, value); e.has_value()) return e;
    if (auto e = push(KvJournalOp::TxnEnd, {}, {}); e.has_value()) return e;
    fh_.journal_end = off;
    f.seekp(static_cast<std::streamoff>(header_offset_), std::ios::beg);
    f.write(reinterpret_cast<const char*>(&fh_), sizeof(fh_));
    if (!f) return Error{Errc::Io, "header update failed"};
    return std::nullopt;
  }

  Expected<std::uint64_t> write_kv_record_(std::fstream& f,
                                           std::uint64_t at,
                                           KvJournalOp op,
                                           std::string_view key,
                                           std::string_view value,
                                           std::uint64_t txn_id) {
    KvJournalRecordHeader h{};
    h.op = static_cast<std::uint8_t>(op);
    h.key_len = static_cast<std::uint32_t>(key.size());
    h.value_len = static_cast<std::uint32_t>(value.size());
    h.lsn = next_lsn_++;
    h.txn_id = txn_id;
    f.seekp(static_cast<std::streamoff>(at), std::ios::beg);
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (h.key_len) f.write(key.data(), static_cast<std::streamsize>(h.key_len));
    if (h.value_len) f.write(value.data(), static_cast<std::streamsize>(h.value_len));
    if (!f) return Error{Errc::Io, "journal write failed"};
    return at + sizeof(h) + h.key_len + h.value_len;
  }

  Expected<void> replay_journal_(std::map<std::string, std::string>& target) const {
    target.clear();
    if (fh_.journal_end < fh_.journal_begin) return Error{Errc::Corrupt, "journal offsets"};
    if (fh_.journal_end == fh_.journal_begin) return std::nullopt;

    std::ifstream f(path_, std::ios::binary);
    if (!f) return Error{Errc::Io, "cannot open file for journal read"};
    f.seekg(static_cast<std::streamoff>(fh_.journal_begin), std::ios::beg);
    std::uint64_t off = fh_.journal_begin;
    std::uint64_t last_lsn = 0;
    std::uint64_t max_txn = 0;
    std::unordered_map<std::uint64_t, std::vector<std::pair<KvJournalOp, std::pair<std::string, std::string>>>> pending;
    while (off < fh_.journal_end) {
      KvJournalRecordHeader h{};
      f.read(reinterpret_cast<char*>(&h), sizeof(h));
      if (!f) break; // crash-tail tolerance: incomplete trailing record
      off += sizeof(h);
      if (off + h.key_len + h.value_len > fh_.journal_end) return Error{Errc::Corrupt, "journal entry bounds"};
      std::string key(h.key_len, '\0');
      std::string value(h.value_len, '\0');
      if (h.key_len) f.read(key.data(), static_cast<std::streamsize>(h.key_len));
      if (h.value_len) f.read(value.data(), static_cast<std::streamsize>(h.value_len));
      if (!f) break; // crash-tail tolerance: incomplete payload
      off += h.key_len + h.value_len;
      if (h.lsn <= last_lsn) return Error{Errc::Corrupt, "non-monotonic lsn"};
      last_lsn = h.lsn;
      const auto op = static_cast<KvJournalOp>(h.op);
      if (h.txn_id > max_txn) max_txn = h.txn_id;
      switch (op) {
        case KvJournalOp::TxnBegin:
          if (pending.find(h.txn_id) != pending.end()) return Error{Errc::Corrupt, "duplicate txn begin"};
          pending.emplace(h.txn_id, std::vector<std::pair<KvJournalOp, std::pair<std::string, std::string>>>{});
          break;
        case KvJournalOp::Put:
        case KvJournalOp::Erase: {
          auto it = pending.find(h.txn_id);
          if (it == pending.end()) return Error{Errc::Corrupt, "orphan journal op"};
          it->second.emplace_back(op, std::make_pair(std::move(key), std::move(value)));
          break;
        }
        case KvJournalOp::Defragment: {
          auto it = pending.find(h.txn_id);
          if (it == pending.end()) return Error{Errc::Corrupt, "orphan maintenance op"};
          it->second.emplace_back(op, std::make_pair(std::string{}, std::string{}));
          break;
        }
        case KvJournalOp::TxnEnd: {
          auto it = pending.find(h.txn_id);
          if (it == pending.end()) return Error{Errc::Corrupt, "orphan txn end"};
          for (const auto& e : it->second) {
            if (e.first == KvJournalOp::Put) target[e.second.first] = e.second.second;
            else if (e.first == KvJournalOp::Erase) target.erase(e.second.first);
          }
          pending.erase(it);
          break;
        }
        default:
          return Error{Errc::Corrupt, "unknown journal op"};
      }
    }
    next_lsn_ = last_lsn + 1;
    next_txn_id_ = max_txn + 1;
    return std::nullopt;
  }

  Expected<void> load_kv_journal_() { return replay_journal_(kv_index_); }

  static std::string embedded_python_stub_() {
    // Codex hint: Keep the embedded script *valid python* and make it tolerant.
    // This is a stub; later we parse the binary and implement commands.
    return R"PY(
# --- ZethaSTORAGE embedded helper (stub) ---
# This region is embedded inside .zdb files.
# The binary header contains python_begin/python_end offsets.
#
# Usage examples:
#   ./foo.zdb print-header --json --sink=$stdout
#   ./foo.zdb defragment --helper=defrag-helper.py
#   ./foo.zdb rebuild --journal=foo.zdbj
#   ./foo.zdb compress --zip --out=foo.zip
#   ./foo.zdb query --file=myquery.zdq
#   ./foo.zdb cron --time="2AM" --daily --action="backup@$BACKUP_DIR"
#   ./foo.zdb clean
#   ./foo.zdb encrypt --password

import sys, json, argparse, base64, os, struct

def main(argv):
    ap = argparse.ArgumentParser(prog=os.path.basename(argv[0]))
    sub = ap.add_subparsers(dest="cmd")

    sub.add_parser("print-header")
    sub.add_parser("defragment")
    sub.add_parser("rebuild")
    sub.add_parser("compress")
    sub.add_parser("query")
    sub.add_parser("cron")
    sub.add_parser("clean")
    sub.add_parser("encrypt")
    sub.add_parser("help")

    ns, rest = ap.parse_known_args(argv[1:])

    if ns.cmd in (None, "help"):
        ap.print_help()
        return 0

    # Stub behavior only:
    if ns.cmd == "print-header":
        print(json.dumps({"stub": True, "note": "header parsing not implemented in embedded script"}, indent=2))
        return 0

    print(json.dumps({"stub": True, "cmd": ns.cmd, "args": rest}))
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
)PY";
  }

private:
  std::string path_;
  bool has_shebang_ = false;
  std::uint64_t header_offset_ = 0;

  FileHeaderFixed fh_{};
  std::vector<std::byte> magic_;
  std::uint32_t page_size_ = 0;
  std::uint32_t block_size_ = 0;

  Schema schema_;
  std::unique_ptr<PageCache> cache_;
  std::map<std::string, std::string> kv_index_;
  mutable std::uint64_t next_lsn_ = 1;
  mutable std::uint64_t next_txn_id_ = 1;

  // future: page map index, open file handle, mmap windows, journaling state, etc.
};

} // namespace zetha::storage
