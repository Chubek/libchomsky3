#pragma once

/*
  ZethaRDB.hpp
  ------------
  Minimal relational DB façade for ZethaStorage + ZethaQUERY + IPCtk.

  Goals:
    - create/open/delete .zdb databases using ZethaStorage
    - initialize schema from textual/native schema source
    - CRUD rows via simple API and query API
    - accept ZethaQUERY textual/native queries
    - remain header-only
    - expose optional IPC helpers through IPCtk

  Notes:
    - This is intentionally lightweight and not a full SQL engine.
    - Storage layout is logical/keyed rather than page-engine specific.
    - Query execution is adapter-based and conservative because the
      provided ZethaQUERY is path-oriented and ZethaSTORAGE low-level
      CRUD details were only partially surfaced in retrieval.
*/

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ZethaQUERY.hpp"
#include "ZethaSTORAGE.hpp"
#include "IPCtk/IPCtk.hpp"

namespace zetha::rdb {
using ZethaStorage = zetha::storage::ZethaStorage;

// ============================================================
// 1) Utility types
// ============================================================

using RowId = std::string;

enum class ValueKind : std::uint8_t {
  Null,
  Integer,
  Real,
  Boolean,
  Text,
  Blob
};

struct Value {
  ValueKind kind{ValueKind::Null};
  std::string data; // canonical storage form

  static Value Null() { return {}; }
  static Value Integer(std::int64_t v) { return {ValueKind::Integer, std::to_string(v)}; }
  static Value Real(double v) {
    std::ostringstream oss;
    oss << v;
    return {ValueKind::Real, oss.str()};
  }
  static Value Boolean(bool v) { return {ValueKind::Boolean, v ? "true" : "false"}; }
  static Value Text(std::string v) { return {ValueKind::Text, std::move(v)}; }
  static Value Blob(std::string v) { return {ValueKind::Blob, std::move(v)}; }

  bool is_null() const { return kind == ValueKind::Null; }

  std::int64_t as_i64(std::int64_t fallback = 0) const {
    try { return std::stoll(data); } catch (...) { return fallback; }
  }
  double as_f64(double fallback = 0.0) const {
    try { return std::stod(data); } catch (...) { return fallback; }
  }
  bool as_bool(bool fallback = false) const {
    if (data == "true" || data == "1") return true;
    if (data == "false" || data == "0") return false;
    return fallback;
  }
  const std::string& as_text() const { return data; }
};

using Row = std::unordered_map<std::string, Value>;

struct Column {
  std::string name;
  ValueKind type{ValueKind::Text};
  bool nullable{true};
  bool primary_key{false};
  bool unique{false};
  std::optional<Value> default_value;
};

struct TableSchema {
  std::string name;
  std::vector<Column> columns;
  std::string primary_key; // optional but recommended

  const Column* find_column(std::string_view n) const {
    for (const auto& c : columns) if (c.name == n) return &c;
    return nullptr;
  }
};

struct Schema {
  std::string database_name;
  std::vector<TableSchema> tables;

  const TableSchema* find_table(std::string_view n) const {
    for (const auto& t : tables) if (t.name == n) return &t;
    return nullptr;
  }
  TableSchema* find_table(std::string_view n) {
    for (auto& t : tables) if (t.name == n) return &t;
    return nullptr;
  }
};

// ============================================================
// 2) Error/result layer
// ============================================================

enum class RdbErrc : std::uint16_t {
  Ok = 0,
  InvalidArgument,
  SchemaError,
  TableNotFound,
  ColumnNotFound,
  PrimaryKeyMissing,
  DuplicateKey,
  RowNotFound,
  QueryParseError,
  QueryExecutionError,
  StorageError,
  IoError,
  Unsupported,
  IpcError
};

struct Error {
  RdbErrc code{RdbErrc::Ok};
  std::string message;

  explicit operator bool() const { return code != RdbErrc::Ok; }
};

template <class T>
struct Result {
  std::optional<T> value;
  std::optional<Error> error;

  explicit operator bool() const { return value.has_value() && !error.has_value(); }

  static Result ok(T v) {
    Result r; r.value = std::move(v); return r;
  }
  static Result fail(RdbErrc c, std::string m) {
    Result r; r.error = Error{c, std::move(m)}; return r;
  }
};

template <>
struct Result<void> {
  std::optional<Error> error;

  explicit operator bool() const { return !error.has_value(); }

  static Result ok() { return {}; }
  static Result fail(RdbErrc c, std::string m) {
    Result r; r.error = Error{c, std::move(m)}; return r;
  }
};

// ============================================================
// 3) Minimal schema DSL support
// ============================================================

/*
  Native DSL:
    auto s = schema("appdb")
      .table("users", {
         col("id", ValueKind::Text).primary_key(),
         col("name", ValueKind::Text),
         col("age", ValueKind::Integer).nullable_col(true)
      });

  Text DSL (minimal):
    database appdb;
    table users (
      id text primary key,
      name text,
      age integer
    );
*/

namespace schema_dsl {

struct ColumnBuilder {
  Column c;

  ColumnBuilder(std::string name, ValueKind kind) {
    c.name = std::move(name);
    c.type = kind;
  }

  ColumnBuilder& nullable_col(bool v = true) { c.nullable = v; return *this; }
  ColumnBuilder& not_null() { c.nullable = false; return *this; }
  ColumnBuilder& primary_key() { c.primary_key = true; c.unique = true; c.nullable = false; return *this; }
  ColumnBuilder& unique() { c.unique = true; return *this; }
  ColumnBuilder& default_to(Value v) { c.default_value = std::move(v); return *this; }

  operator Column() const { return c; }
};

inline ColumnBuilder col(std::string name, ValueKind kind) {
  return ColumnBuilder(std::move(name), kind);
}

struct SchemaBuilder {
  Schema s;

  explicit SchemaBuilder(std::string dbname) { s.database_name = std::move(dbname); }

  SchemaBuilder& table(std::string name, std::initializer_list<Column> cols) {
    TableSchema t;
    t.name = std::move(name);
    t.columns.assign(cols.begin(), cols.end());
    for (const auto& c : t.columns) {
      if (c.primary_key) {
        if (!t.primary_key.empty()) {
          throw std::logic_error("multiple primary keys not supported");
        }
        t.primary_key = c.name;
      }
    }
    s.tables.push_back(std::move(t));
    return *this;
  }

  Schema build() const { return s; }
};

inline SchemaBuilder schema(std::string dbname) {
  return SchemaBuilder(std::move(dbname));
}

} // namespace schema_dsl

class TextSchemaParser {
public:
  static Result<Schema> parse(std::string_view text) {
    // Minimal forgiving parser, intentionally small.
    // It supports:
    //   database NAME;
    //   table NAME ( col type [primary key] [unique] [not null], ... );

    std::string src(text);
    auto norm = normalize_ws(src);

    Schema out;
    std::size_t pos = 0;

    if (!consume_kw(norm, pos, "database")) {
      return Result<Schema>::fail(RdbErrc::SchemaError, "expected 'database'");
    }

    auto dbname = read_ident(norm, pos);
    if (dbname.empty()) {
      return Result<Schema>::fail(RdbErrc::SchemaError, "expected database name");
    }
    out.database_name = dbname;
    consume_char(norm, pos, ';');

    while (true) {
      skip_ws(norm, pos);
      if (pos >= norm.size()) break;

      if (!consume_kw(norm, pos, "table")) {
        return Result<Schema>::fail(RdbErrc::SchemaError, "expected 'table'");
      }

      TableSchema table;
      table.name = read_ident(norm, pos);
      if (table.name.empty()) {
        return Result<Schema>::fail(RdbErrc::SchemaError, "expected table name");
      }

      if (!consume_char(norm, pos, '(')) {
        return Result<Schema>::fail(RdbErrc::SchemaError, "expected '(' after table name");
      }

      while (true) {
        skip_ws(norm, pos);
        if (consume_char(norm, pos, ')')) break;

        Column col;
        col.name = read_ident(norm, pos);
        if (col.name.empty()) {
          return Result<Schema>::fail(RdbErrc::SchemaError, "expected column name");
        }

        auto ty = read_ident(norm, pos);
        if (ty.empty()) {
          return Result<Schema>::fail(RdbErrc::SchemaError, "expected column type");
        }
        auto kind = parse_type(ty);
        if (!kind.has_value()) {
          return Result<Schema>::fail(RdbErrc::SchemaError, "unknown type: " + ty);
        }
        col.type = *kind;

        for (;;) {
          auto save = pos;
          if (consume_kw(norm, pos, "primary")) {
            if (!consume_kw(norm, pos, "key")) {
              return Result<Schema>::fail(RdbErrc::SchemaError, "expected 'key' after 'primary'");
            }
            col.primary_key = true;
            col.unique = true;
            col.nullable = false;
            continue;
          }
          pos = save;
          if (consume_kw(norm, pos, "unique")) {
            col.unique = true;
            continue;
          }
          pos = save;
          if (consume_kw(norm, pos, "not")) {
            if (!consume_kw(norm, pos, "null")) {
              return Result<Schema>::fail(RdbErrc::SchemaError, "expected 'null' after 'not'");
            }
            col.nullable = false;
            continue;
          }
          pos = save;
          break;
        }

        table.columns.push_back(col);
        if (col.primary_key) {
          if (!table.primary_key.empty()) {
            return Result<Schema>::fail(RdbErrc::SchemaError, "multiple primary keys not supported");
          }
          table.primary_key = col.name;
        }

        skip_ws(norm, pos);
        if (consume_char(norm, pos, ',')) continue;
        if (consume_char(norm, pos, ')')) break;

        return Result<Schema>::fail(RdbErrc::SchemaError, "expected ',' or ')'");
      }

      consume_char(norm, pos, ';');
      out.tables.push_back(std::move(table));
    }

    return Result<Schema>::ok(std::move(out));
  }

private:
  static std::string normalize_ws(const std::string& s) {
    return s;
  }

  static void skip_ws(const std::string& s, std::size_t& pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
  }

  static bool consume_char(const std::string& s, std::size_t& pos, char ch) {
    skip_ws(s, pos);
    if (pos < s.size() && s[pos] == ch) { ++pos; return true; }
    return false;
  }

  static bool consume_kw(const std::string& s, std::size_t& pos, const char* kw) {
    skip_ws(s, pos);
    std::size_t n = std::char_traits<char>::length(kw);
    if (s.substr(pos, n) == kw) {
      std::size_t end = pos + n;
      if (end == s.size() || !std::isalnum(static_cast<unsigned char>(s[end]))) {
        pos = end;
        return true;
      }
    }
    return false;
  }

  static std::string read_ident(const std::string& s, std::size_t& pos) {
    skip_ws(s, pos);
    std::size_t start = pos;
    if (pos < s.size() && (std::isalpha(static_cast<unsigned char>(s[pos])) || s[pos] == '_')) {
      ++pos;
      while (pos < s.size()) {
        char c = s[pos];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ++pos;
        else break;
      }
    }
    return s.substr(start, pos - start);
  }

  static std::optional<ValueKind> parse_type(const std::string& s) {
    if (s == "int" || s == "integer" || s == "i64") return ValueKind::Integer;
    if (s == "real" || s == "float" || s == "double") return ValueKind::Real;
    if (s == "bool" || s == "boolean") return ValueKind::Boolean;
    if (s == "text" || s == "string") return ValueKind::Text;
    if (s == "blob" || s == "bytes") return ValueKind::Blob;
    if (s == "null") return ValueKind::Null;
    return std::nullopt;
  }
};

// ============================================================
// 4) Row serialization
// ============================================================

namespace detail {

inline std::string escape_json(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += c; break;
    }
  }
  return out;
}

inline std::string value_kind_name(ValueKind k) {
  switch (k) {
    case ValueKind::Null: return "null";
    case ValueKind::Integer: return "integer";
    case ValueKind::Real: return "real";
    case ValueKind::Boolean: return "boolean";
    case ValueKind::Text: return "text";
    case ValueKind::Blob: return "blob";
  }
  return "text";
}

inline std::optional<ValueKind> value_kind_from_name(std::string_view s) {
  if (s == "null") return ValueKind::Null;
  if (s == "integer") return ValueKind::Integer;
  if (s == "real") return ValueKind::Real;
  if (s == "boolean") return ValueKind::Boolean;
  if (s == "text") return ValueKind::Text;
  if (s == "blob") return ValueKind::Blob;
  return std::nullopt;
}

inline std::string escape_field(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\\' || c == '\n' || c == '\t') out.push_back('\\');
    out.push_back(c == '\n' ? 'n' : (c == '\t' ? 't' : c));
  }
  return out;
}

inline std::optional<std::string> unescape_field(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c != '\\') {
      out.push_back(c);
      continue;
    }
    if (i + 1 >= s.size()) return std::nullopt;
    const char n = s[++i];
    if (n == 'n') out.push_back('\n');
    else if (n == 't') out.push_back('\t');
    else out.push_back(n);
  }
  return out;
}

inline std::vector<std::string_view> split_tab(std::string_view line) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= line.size(); ++i) {
    if (i == line.size() || line[i] == '\t') {
      out.push_back(line.substr(start, i - start));
      start = i + 1;
    }
  }
  return out;
}

inline std::string encode_row(const Row& row) {
  std::ostringstream oss;
  oss << "ZRDBROW1\n";
  for (const auto& [k, v] : row) {
    oss << escape_field(k) << '\t'
        << value_kind_name(v.kind) << '\t'
        << escape_field(v.data) << '\n';
  }
  return oss.str();
}

inline std::string encode_schema(const Schema& s) {
  std::ostringstream oss;
  oss << "ZRDBSCHEMA1\n";
  oss << "DB\t" << escape_field(s.database_name) << '\n';
  for (const auto& t : s.tables) {
    oss << "TABLE\t" << escape_field(t.name) << '\t' << escape_field(t.primary_key) << '\n';
    for (const auto& c : t.columns) {
      oss << "COL\t" << escape_field(c.name) << '\t' << value_kind_name(c.type) << '\t'
          << (c.nullable ? "1" : "0") << '\t'
          << (c.primary_key ? "1" : "0") << '\t'
          << (c.unique ? "1" : "0") << '\n';
    }
    oss << "ENDTABLE\n";
  }
  return oss.str();
}

inline Result<Row> decode_row(std::string_view encoded) {
  std::istringstream in{std::string(encoded)};
  std::string line;
  if (!std::getline(in, line) || line != "ZRDBROW1") {
    return Result<Row>::fail(RdbErrc::QueryExecutionError, "row codec magic mismatch");
  }
  Row out;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split_tab(line);
    if (parts.size() != 3) return Result<Row>::fail(RdbErrc::QueryExecutionError, "row codec malformed line");
    auto key = unescape_field(parts[0]);
    auto kind = value_kind_from_name(parts[1]);
    auto data = unescape_field(parts[2]);
    if (!key || !kind || !data) return Result<Row>::fail(RdbErrc::QueryExecutionError, "row codec decode failed");
    out[*key] = Value{*kind, std::move(*data)};
  }
  return Result<Row>::ok(std::move(out));
}

inline Result<Schema> decode_schema(std::string_view encoded) {
  std::istringstream in{std::string(encoded)};
  std::string line;
  if (!std::getline(in, line) || line != "ZRDBSCHEMA1") {
    return Result<Schema>::fail(RdbErrc::SchemaError, "schema codec magic mismatch");
  }

  Schema s;
  TableSchema* current_table = nullptr;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto parts = split_tab(line);
    if (parts.empty()) continue;
    if (parts[0] == "DB") {
      if (parts.size() != 2) return Result<Schema>::fail(RdbErrc::SchemaError, "bad DB record");
      auto name = unescape_field(parts[1]);
      if (!name) return Result<Schema>::fail(RdbErrc::SchemaError, "bad DB name");
      s.database_name = *name;
      continue;
    }
    if (parts[0] == "TABLE") {
      if (parts.size() != 3) return Result<Schema>::fail(RdbErrc::SchemaError, "bad TABLE record");
      auto tname = unescape_field(parts[1]);
      auto pkey = unescape_field(parts[2]);
      if (!tname || !pkey) return Result<Schema>::fail(RdbErrc::SchemaError, "bad TABLE decode");
      s.tables.push_back(TableSchema{});
      current_table = &s.tables.back();
      current_table->name = std::move(*tname);
      current_table->primary_key = std::move(*pkey);
      continue;
    }
    if (parts[0] == "COL") {
      if (!current_table) return Result<Schema>::fail(RdbErrc::SchemaError, "COL without TABLE");
      if (parts.size() != 6) return Result<Schema>::fail(RdbErrc::SchemaError, "bad COL record");
      auto cname = unescape_field(parts[1]);
      auto ctype = value_kind_from_name(parts[2]);
      if (!cname || !ctype) return Result<Schema>::fail(RdbErrc::SchemaError, "bad COL decode");
      Column c;
      c.name = std::move(*cname);
      c.type = *ctype;
      c.nullable = parts[3] == "1";
      c.primary_key = parts[4] == "1";
      c.unique = parts[5] == "1";
      current_table->columns.push_back(std::move(c));
      continue;
    }
    if (parts[0] == "ENDTABLE") {
      current_table = nullptr;
      continue;
    }
    return Result<Schema>::fail(RdbErrc::SchemaError, "unknown schema record");
  }
  return Result<Schema>::ok(std::move(s));
}

inline std::string table_prefix(std::string_view table) {
  return "tbl:" + std::string(table) + ":";
}

inline std::string row_key(std::string_view table, std::string_view pk) {
  return table_prefix(table) + std::string(pk);
}

inline std::string schema_key() { return "__zetha_rdb_schema__"; }
inline std::string meta_key()   { return "__zetha_rdb_meta__"; }

template <class Storage, class = void>
struct has_insert : std::false_type {};

template <class Storage>
struct has_insert<Storage, std::void_t<
  decltype(std::declval<Storage&>().insert(std::declval<std::string_view>(),
                                           std::declval<std::string_view>()))
>> : std::true_type {};

template <class Storage, class = void>
struct has_get : std::false_type {};

template <class Storage>
struct has_get<Storage, std::void_t<
  decltype(std::declval<Storage&>().get(std::declval<std::string_view>()))
>> : std::true_type {};

template <class Storage, class = void>
struct has_erase : std::false_type {};

template <class Storage>
struct has_erase<Storage, std::void_t<
  decltype(std::declval<Storage&>().erase(std::declval<std::string_view>()))
>> : std::true_type {};

} // namespace detail

// ============================================================
// 5) Database options
// ============================================================

struct CreateOptions {
  std::size_t page_size{4096};
  std::string schema_text;
  std::optional<Schema> schema_native;
};

struct OpenOptions {
  bool read_only{false};
};

struct QueryOptions {
  std::optional<std::size_t> limit;
  std::optional<std::size_t> offset;
};

// ============================================================
// 6) Query result
// ============================================================

struct QueryResult {
  std::vector<Row> rows;
  std::size_t affected{0};
};

// ============================================================
// 7) ZethaRDB
// ============================================================

class ZethaRDB {
public:
  using StorageType = ZethaStorage;

  ZethaRDB() = default;

  // ------------------------------------------------------------
  // lifecycle
  // ------------------------------------------------------------

  static Result<ZethaRDB> create(const std::string& path, const CreateOptions& opt = {}) {
    ZethaRDB db;
    db.path_ = path;

    auto schema_res = resolve_schema(opt);
    if (!schema_res) return Result<ZethaRDB>::fail(schema_res.error->code, schema_res.error->message);

    db.schema_ = *schema_res.value;

    zetha::storage::CreateOptions sopt{};
    sopt.page_size = opt.page_size;
    sopt.schema.json_schema = detail::encode_schema(db.schema_);

    auto created = ZethaStorage::create(path, sopt);
    if (std::holds_alternative<zetha::storage::Error>(created)) {
      return Result<ZethaRDB>::fail(RdbErrc::StorageError, "ZethaStorage::create failed");
    }

    db.storage_ = std::move(std::get<ZethaStorage>(created));
    auto init_res = db.persist_schema();
    if (!init_res) return Result<ZethaRDB>::fail(init_res.error->code, init_res.error->message);

    return Result<ZethaRDB>::ok(std::move(db));
  }

  static Result<ZethaRDB> open(const std::string& path, const OpenOptions& opt = {}) {
    ZethaRDB db;
    db.path_ = path;
    db.read_only_ = opt.read_only;

    zetha::storage::OpenOptions sopt{};
    sopt.read_only = opt.read_only;

    auto opened = ZethaStorage::open(path, sopt);
    if (std::holds_alternative<zetha::storage::Error>(opened)) {
      return Result<ZethaRDB>::fail(RdbErrc::StorageError, "ZethaStorage::open failed");
    }

    db.storage_ = std::move(std::get<ZethaStorage>(opened));

    // Best effort: schema may be reconstructed or supplied later.
    // If storage::get exists, we try to load persisted schema payload.
    auto load_res = db.try_load_schema();
    if (!load_res && load_res.error->code != RdbErrc::Unsupported) {
      return Result<ZethaRDB>::fail(load_res.error->code, load_res.error->message);
    }

    return Result<ZethaRDB>::ok(std::move(db));
  }

  static Result<void> remove(const std::string& path) {
    std::error_code ec;
    bool ok = std::filesystem::remove(path, ec);
    if (ec) return Result<void>::fail(RdbErrc::IoError, ec.message());
    if (!ok) return Result<void>::fail(RdbErrc::IoError, "database file not removed");
    return Result<void>::ok();
  }

  const std::string& path() const noexcept { return path_; }
  const Schema& schema() const noexcept { return schema_; }

  // ------------------------------------------------------------
  // schema init
  // ------------------------------------------------------------

  Result<void> init_schema(const Schema& s) {
    if (read_only_) return Result<void>::fail(RdbErrc::Unsupported, "database opened read-only");
    schema_ = s;
    return persist_schema();
  }

  Result<void> init_schema(std::string_view text_schema) {
    auto parsed = TextSchemaParser::parse(text_schema);
    if (!parsed) return Result<void>::fail(RdbErrc::SchemaError, "schema parse failed");
    return init_schema(parsed.value.value());
  }

  // ------------------------------------------------------------
  // CRUD
  // ------------------------------------------------------------

  Result<RowId> insert(std::string_view table_name, Row row) {
    auto table = schema_.find_table(table_name);
    if (!table) return Result<RowId>::fail(RdbErrc::TableNotFound, "table not found");

    auto norm = normalize_row_for_insert(*table, std::move(row));
    if (!norm) return Result<RowId>::fail(norm.error->code, norm.error->message);

    Row ready = std::move(*norm.value);
    auto pk_it = ready.find(table->primary_key);
    if (pk_it == ready.end()) {
      return Result<RowId>::fail(RdbErrc::PrimaryKeyMissing, "primary key missing");
    }

    const std::string pk = pk_it->second.data;
    const std::string key = detail::row_key(table_name, pk);
    const std::string payload = detail::encode_row(ready);

    auto write = storage_insert(key, payload);
    if (!write) return Result<RowId>::fail(write.error->code, write.error->message);

    return Result<RowId>::ok(pk);
  }

  Result<std::optional<Row>> get(std::string_view table_name, std::string_view pk) {
    auto table = schema_.find_table(table_name);
    if (!table) return Result<std::optional<Row>>::fail(RdbErrc::TableNotFound, "table not found");

    const std::string key = detail::row_key(table_name, pk);
    auto raw = storage_get(key);
    if (!raw) return Result<std::optional<Row>>::fail(raw.error->code, raw.error->message);
    if (!raw.value->has_value()) return Result<std::optional<Row>>::ok(std::nullopt);

    auto decoded = detail::decode_row(**raw.value);
    if (!decoded) return Result<std::optional<Row>>::fail(decoded.error->code, decoded.error->message);
    return Result<std::optional<Row>>::ok(std::move(*decoded.value));
  }

  Result<void> erase(std::string_view table_name, std::string_view pk) {
    auto table = schema_.find_table(table_name);
    if (!table) return Result<void>::fail(RdbErrc::TableNotFound, "table not found");

    const std::string key = detail::row_key(table_name, pk);
    return storage_erase(key);
  }

  Result<std::size_t> update(std::string_view table_name, std::string_view pk, const Row& patch) {
    auto found = get(table_name, pk);
    if (!found) return Result<std::size_t>::fail(found.error->code, found.error->message);
    if (!found.value->has_value()) return Result<std::size_t>::fail(RdbErrc::RowNotFound, "row not found");

    Row updated = **found.value;

    // If loaded through raw fallback, we cannot structurally patch safely.
    // Minimal behavior: create replacement row from patch plus PK.
    updated = patch;
    auto table = schema_.find_table(table_name);
    if (!table) return Result<std::size_t>::fail(RdbErrc::TableNotFound, "table not found");
    updated[table->primary_key] = Value::Text(std::string(pk));

    auto norm = normalize_row_for_insert(*table, std::move(updated));
    if (!norm) return Result<std::size_t>::fail(norm.error->code, norm.error->message);

    auto payload = detail::encode_row(*norm.value);
    auto wr = storage_insert(detail::row_key(table_name, pk), payload);
    if (!wr) return Result<std::size_t>::fail(wr.error->code, wr.error->message);

    return Result<std::size_t>::ok(1);
  }

  // ------------------------------------------------------------
  // query integration
  // ------------------------------------------------------------

  Result<QueryResult> query(const zetha::query::Query& q, const QueryOptions& opt = {}) {
    // ZethaQUERY is path-oriented. Minimal relational mapping:
    //   /table           => scan table
    //   /table/@id       => project attribute path (not materialized here)
    //   predicates are accepted structurally but full evaluation is conservative.
    //
    // Since the provided storage retrieval does not expose iteration/scan APIs,
    // this adapter validates and returns a shaped execution plan result.
    //
    // If you want, next I can extend this with an in-memory secondary catalog /
    // row index journal for true table scans.

    QueryResult out;

    if (q.path.steps.empty()) {
      return Result<QueryResult>::fail(RdbErrc::QueryExecutionError, "empty query path");
    }

    const auto& first = q.path.steps.front();
    if (first.test.kind != zetha::query::NodeTestKind::Name) {
      return Result<QueryResult>::fail(RdbErrc::QueryExecutionError, "first path step must name a table");
    }

    const std::string& table_name = first.test.name;
    auto table = schema_.find_table(table_name);
    if (!table) {
      return Result<QueryResult>::fail(RdbErrc::TableNotFound, "table not found: " + table_name);
    }

    if (!storage_.has_value()) {
      return Result<QueryResult>::fail(RdbErrc::StorageError, "storage not open");
    }
    if (q.path.steps.size() != 1) {
      return Result<QueryResult>::fail(RdbErrc::Unsupported, "only single-step table query is supported");
    }

    std::size_t emitted = 0;
    std::size_t skipped = 0;
    const auto prefix = detail::table_prefix(table_name);
    for (auto it = storage_->scan_begin(); it.valid(); it.next()) {
      const auto item = it.item();
      if (item.key.rfind(prefix, 0) != 0) continue;

      auto decoded = detail::decode_row(item.value);
      if (!decoded) return Result<QueryResult>::fail(decoded.error->code, decoded.error->message);
      if (!row_matches_step_(*decoded.value, first)) continue;

      if (opt.offset.has_value() && skipped < *opt.offset) {
        ++skipped;
        continue;
      }
      out.rows.push_back(std::move(*decoded.value));
      ++emitted;
      if (opt.limit.has_value() && emitted >= *opt.limit) break;
    }
    out.affected = out.rows.size();
    return Result<QueryResult>::ok(std::move(out));
  }

  Result<QueryResult> query(std::string_view textual_query, const QueryOptions& opt = {}) {
    zetha::query::text::Parser p;
    auto parsed = p.parse(textual_query);
    if (parsed.is_err()) {
      return Result<QueryResult>::fail(RdbErrc::QueryParseError, "query parse failed");
    }
    return query(parsed.unwrap(), opt);
  }

  // ------------------------------------------------------------
  // IPC integration
  // ------------------------------------------------------------

  struct IPCMessage {
    std::string verb;      // e.g. insert/get/query/delete
    std::string table;
    std::string payload;
  };

  struct IPCSerde {
    static std::string encode(const IPCMessage& m) {
      std::ostringstream oss;
      oss << "ZRDBIPC1\n"
          << detail::escape_field(m.verb) << '\n'
          << detail::escape_field(m.table) << '\n'
          << detail::escape_field(m.payload) << '\n';
      return oss.str();
    }
    static Result<IPCMessage> decode(std::string_view encoded) {
      std::istringstream in{std::string(encoded)};
      std::string magic;
      std::string verb;
      std::string table;
      std::string payload;
      if (!std::getline(in, magic) || magic != "ZRDBIPC1" ||
          !std::getline(in, verb) || !std::getline(in, table) || !std::getline(in, payload)) {
        return Result<IPCMessage>::fail(RdbErrc::IpcError, "invalid ipc message");
      }
      auto dverb = detail::unescape_field(verb);
      auto dtable = detail::unescape_field(table);
      auto dpayload = detail::unescape_field(payload);
      if (!dverb || !dtable || !dpayload) {
        return Result<IPCMessage>::fail(RdbErrc::IpcError, "invalid ipc field encoding");
      }
      return Result<IPCMessage>::ok(IPCMessage{std::move(*dverb), std::move(*dtable), std::move(*dpayload)});
    }
  };

  static std::string encode_ipc_message(const IPCMessage& m) {
    return IPCSerde::encode(m);
  }

  template <class Channel>
  static Result<void> ipc_send(Channel& ch, const IPCMessage& msg) {
    const std::string bytes = IPCSerde::encode(msg);

    // IPCtk docs surfaced raw byte send/receive semantics.
    // We avoid assuming stronger serde support.
    auto r = ch.send(bytes.data(), bytes.size());
    if (!r) return Result<void>::fail(RdbErrc::IpcError, "IPC send failed");
    return Result<void>::ok();
  }

  template <class Channel>
  static Result<std::string> ipc_receive(Channel& ch, std::size_t max_bytes = 1 << 20) {
    std::string buf;
    buf.resize(max_bytes);

    auto r = ch.receive(buf.data(), buf.size());
    if (!r) return Result<std::string>::fail(RdbErrc::IpcError, "IPC receive failed");

    std::size_t n = static_cast<std::size_t>(*r);
    buf.resize(n);
    return Result<std::string>::ok(std::move(buf));
  }

private:
  std::string path_;
  bool read_only_{false};
  Schema schema_{};
  std::optional<ZethaStorage> storage_;

  // ------------------------------------------------------------
  // schema helpers
  // ------------------------------------------------------------

  static Result<Schema> resolve_schema(const CreateOptions& opt) {
    if (opt.schema_native.has_value()) {
      return Result<Schema>::ok(*opt.schema_native);
    }
    if (!opt.schema_text.empty()) {
      return TextSchemaParser::parse(opt.schema_text);
    }
    Schema s;
    s.database_name = "zetha_rdb";
    return Result<Schema>::ok(std::move(s));
  }

  Result<void> persist_schema() {
    auto payload = detail::encode_schema(schema_);
    return storage_insert(detail::schema_key(), payload);
  }

  Result<void> try_load_schema() {
    auto raw = storage_get(detail::schema_key());
    if (!raw) return Result<void>::fail(raw.error->code, raw.error->message);
    if (!raw.value->has_value()) return Result<void>::fail(RdbErrc::Unsupported, "no persisted schema found");

    auto decoded = detail::decode_schema(**raw.value);
    if (!decoded) return Result<void>::fail(decoded.error->code, decoded.error->message);
    schema_ = std::move(*decoded.value);
    return Result<void>::ok();
  }

  static bool compare_values_(const Value& lhs, const Value& rhs, zetha::query::CmpOp op) {
    auto cmp_text = [&](const std::string& a, const std::string& b) {
      if (op == zetha::query::CmpOp::Eq) return a == b;
      if (op == zetha::query::CmpOp::Ne) return a != b;
      if (op == zetha::query::CmpOp::Lt) return a < b;
      if (op == zetha::query::CmpOp::Le) return a <= b;
      if (op == zetha::query::CmpOp::Gt) return a > b;
      if (op == zetha::query::CmpOp::Ge) return a >= b;
      return false;
    };
    if (lhs.kind == ValueKind::Integer || lhs.kind == ValueKind::Real ||
        rhs.kind == ValueKind::Integer || rhs.kind == ValueKind::Real) {
      const double a = lhs.as_f64();
      const double b = rhs.as_f64();
      if (op == zetha::query::CmpOp::Eq) return a == b;
      if (op == zetha::query::CmpOp::Ne) return a != b;
      if (op == zetha::query::CmpOp::Lt) return a < b;
      if (op == zetha::query::CmpOp::Le) return a <= b;
      if (op == zetha::query::CmpOp::Gt) return a > b;
      if (op == zetha::query::CmpOp::Ge) return a >= b;
      return false;
    }
    if (lhs.kind == ValueKind::Boolean || rhs.kind == ValueKind::Boolean) {
      const bool a = lhs.as_bool();
      const bool b = rhs.as_bool();
      if (op == zetha::query::CmpOp::Eq) return a == b;
      if (op == zetha::query::CmpOp::Ne) return a != b;
      if (op == zetha::query::CmpOp::Lt) return a < b;
      if (op == zetha::query::CmpOp::Le) return a <= b;
      if (op == zetha::query::CmpOp::Gt) return a > b;
      if (op == zetha::query::CmpOp::Ge) return a >= b;
      return false;
    }
    return cmp_text(lhs.data, rhs.data);
  }

  static std::optional<Value> eval_rhs_expr_(const zetha::query::Expr& e, const Row& row) {
    return std::visit([&](const auto& node) -> std::optional<Value> {
      using T = std::decay_t<decltype(node)>;
      if constexpr (std::is_same_v<T, zetha::query::Literal>) {
        if (std::holds_alternative<std::monostate>(node.value)) return Value::Null();
        if (auto p = std::get_if<bool>(&node.value)) return Value::Boolean(*p);
        if (auto p = std::get_if<double>(&node.value)) return Value::Real(*p);
        return Value::Text(std::get<std::string>(node.value));
      } else if constexpr (std::is_same_v<T, zetha::query::NameRef>) {
        return Value::Text(node.name);
      } else if constexpr (std::is_same_v<T, zetha::query::AttributeRef>) {
        auto it = row.find(node.name);
        if (it == row.end()) return std::nullopt;
        return it->second;
      }
      return std::nullopt;
    }, e.v);
  }

  static bool eval_predicate_(const zetha::query::Predicate& pred, const Row& row) {
    return std::visit([&](const auto& node) -> bool {
      using T = std::decay_t<decltype(node)>;
      if constexpr (std::is_same_v<T, zetha::query::Compare>) {
        if (!node.lhs || !node.rhs) return false;
        auto lhs_attr = std::get_if<zetha::query::AttributeRef>(&node.lhs->v);
        if (!lhs_attr) return false;
        auto it = row.find(lhs_attr->name);
        if (it == row.end()) return false;
        auto rhs = eval_rhs_expr_(*node.rhs, row);
        if (!rhs) return false;
        return compare_values_(it->second, *rhs, node.op);
      } else if constexpr (std::is_same_v<T, zetha::query::BoolBin>) {
        if (!node.lhs || !node.rhs) return false;
        bool l = eval_predicate_(zetha::query::Predicate{*node.lhs}, row);
        bool r = eval_predicate_(zetha::query::Predicate{*node.rhs}, row);
        return node.op == zetha::query::BoolOp::And ? (l && r) : (l || r);
      } else if constexpr (std::is_same_v<T, zetha::query::NotExpr>) {
        if (!node.inner) return false;
        return !eval_predicate_(zetha::query::Predicate{*node.inner}, row);
      }
      return false;
    }, pred.expr.v);
  }

  static bool row_matches_step_(const Row& row, const zetha::query::Step& step) {
    for (const auto& pred : step.predicates) {
      if (!eval_predicate_(pred, row)) return false;
    }
    return true;
  }

  Result<Row> normalize_row_for_insert(const TableSchema& table, Row row) const {
    Row out;

    for (const auto& col : table.columns) {
      auto it = row.find(col.name);
      if (it == row.end()) {
        if (col.default_value.has_value()) {
          out[col.name] = *col.default_value;
        } else if (col.nullable) {
          out[col.name] = Value::Null();
        } else {
          return Result<Row>::fail(RdbErrc::SchemaError, "missing non-null column: " + col.name);
        }
      } else {
        out[col.name] = it->second;
      }
    }

    for (const auto& [k, _] : row) {
      if (!table.find_column(k)) {
        return Result<Row>::fail(RdbErrc::ColumnNotFound, "unknown column: " + k);
      }
    }

    if (!table.primary_key.empty()) {
      auto pk = out.find(table.primary_key);
      if (pk == out.end() || pk->second.is_null() || pk->second.data.empty()) {
        return Result<Row>::fail(RdbErrc::PrimaryKeyMissing, "missing primary key: " + table.primary_key);
      }
    }

    return Result<Row>::ok(std::move(out));
  }

  // ------------------------------------------------------------
  // storage wrappers
  // ------------------------------------------------------------

  Result<void> storage_insert(std::string_view key, std::string_view value) {
    if (!storage_.has_value()) return Result<void>::fail(RdbErrc::StorageError, "storage not open");

    if constexpr (detail::has_insert<ZethaStorage>::value) {
      auto r = storage_->insert(key, value);
      if (r.has_value()) {
        return Result<void>::fail(RdbErrc::StorageError, "storage insert failed");
      }
      return Result<void>::ok();
    } else {
      return Result<void>::fail(RdbErrc::Unsupported, "ZethaStorage::insert unavailable");
    }
  }

  Result<std::optional<std::string>> storage_get(std::string_view key) {
    if (!storage_.has_value()) {
      return Result<std::optional<std::string>>::fail(RdbErrc::StorageError, "storage not open");
    }

    if constexpr (detail::has_get<ZethaStorage>::value) {
      auto r = storage_->get(key);
      if (std::holds_alternative<zetha::storage::Error>(r)) {
        return Result<std::optional<std::string>>::fail(RdbErrc::StorageError, "storage get failed");
      }
      return Result<std::optional<std::string>>::ok(std::get<std::optional<std::string>>(r));
    } else {
      return Result<std::optional<std::string>>::fail(RdbErrc::Unsupported, "ZethaStorage::get unavailable");
    }
  }

  Result<void> storage_erase(std::string_view key) {
    if (!storage_.has_value()) return Result<void>::fail(RdbErrc::StorageError, "storage not open");

    if constexpr (detail::has_erase<ZethaStorage>::value) {
      auto r = storage_->erase(key);
      if (r.has_value()) {
        return Result<void>::fail(RdbErrc::StorageError, "storage erase failed");
      }
      return Result<void>::ok();
    } else {
      return Result<void>::fail(RdbErrc::Unsupported, "ZethaStorage::erase unavailable");
    }
  }
};

// ============================================================
// 8) Convenience native relational query builders
// ============================================================

namespace dsl {

struct InsertBuilder {
  std::string table_name;
  Row row;

  InsertBuilder(std::string t, Row r) : table_name(std::move(t)), row(std::move(r)) {}
  Result<RowId> exec(ZethaRDB& db) { return db.insert(table_name, std::move(row)); }
};

struct UpdateBuilder {
  std::string table_name;
  std::string pk;
  Row patch;

  Result<std::size_t> exec(ZethaRDB& db) { return db.update(table_name, pk, patch); }
};

struct DeleteBuilder {
  std::string table_name;
  std::string pk;

  Result<void> exec(ZethaRDB& db) { return db.erase(table_name, pk); }
};

inline InsertBuilder insert_into(std::string table, Row row) {
  return InsertBuilder(std::move(table), std::move(row));
}

inline UpdateBuilder update(std::string table, std::string pk, Row patch) {
  return UpdateBuilder{std::move(table), std::move(pk), std::move(patch)};
}

inline DeleteBuilder delete_from(std::string table, std::string pk) {
  return DeleteBuilder{std::move(table), std::move(pk)};
}

} // namespace dsl

} // namespace zetha::rdb
