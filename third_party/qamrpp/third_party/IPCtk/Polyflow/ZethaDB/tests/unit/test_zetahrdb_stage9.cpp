#include "../../ZethaRDB.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

static int failures = 0;
static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << msg << "\n";
    ++failures;
  }
}

int main() {
  using namespace zetha::rdb;
  namespace fs = std::filesystem;

  const auto db_path = (fs::temp_directory_path() / "zetahrdb_stage9.zdb").string();
  std::error_code ec;
  fs::remove(db_path, ec);

  auto schema = schema_dsl::schema("appdb")
      .table("users", {
          schema_dsl::col("id", ValueKind::Text).primary_key(),
          schema_dsl::col("name", ValueKind::Text),
          schema_dsl::col("age", ValueKind::Integer).not_null()
      })
      .build();

  CreateOptions copt{};
  copt.schema_native = schema;
  auto created = ZethaRDB::create(db_path, copt);
  expect(static_cast<bool>(created), "create should succeed");
  if (!created) return EXIT_FAILURE;

  ZethaRDB db = std::move(*created.value);
  Row r1;
  r1["id"] = Value::Text("u1");
  r1["name"] = Value::Text("alice");
  r1["age"] = Value::Integer(31);
  auto ins = db.insert("users", r1);
  expect(static_cast<bool>(ins), "insert should succeed");

  auto got = db.get("users", "u1");
  expect(static_cast<bool>(got), "get should succeed");
  if (got && got.value->has_value()) {
    auto row = got.value->value();
    expect(row["name"].as_text() == "alice", "get should decode stored row");
  }

  auto q1 = db.query("/users[@age=31]");
  expect(static_cast<bool>(q1), "text query should parse+execute");
  if (q1) expect(q1.value->rows.size() == 1, "query should match one row");

  auto reopened = ZethaRDB::open(db_path, {});
  expect(static_cast<bool>(reopened), "open should succeed");
  if (!reopened) return EXIT_FAILURE;
  ZethaRDB db2 = std::move(*reopened.value);
  expect(db2.schema().find_table("users") != nullptr, "schema should reload from storage payload");

  auto q2 = db2.query("/users[@name='alice']", QueryOptions{.limit = 1, .offset = 0});
  expect(static_cast<bool>(q2), "query on reopened db should succeed");
  if (q2) expect(q2.value->rows.size() == 1, "reopened query should match one row");

  auto del = db2.erase("users", "u1");
  expect(static_cast<bool>(del), "erase should succeed");
  auto q3 = db2.query("/users[@id='u1']");
  expect(static_cast<bool>(q3), "query after erase should succeed");
  if (q3) expect(q3.value->rows.empty(), "query after erase should be empty");

  auto ipc = ZethaRDB::IPCMessage{"query", "users", "/users[@age=31]"};
  auto bytes = ZethaRDB::encode_ipc_message(ipc);
  auto decoded = ZethaRDB::IPCSerde::decode(bytes);
  expect(static_cast<bool>(decoded), "ipc serde decode should succeed");
  if (decoded) expect(decoded.value->payload == ipc.payload, "ipc serde payload roundtrip");

  fs::remove(db_path, ec);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
