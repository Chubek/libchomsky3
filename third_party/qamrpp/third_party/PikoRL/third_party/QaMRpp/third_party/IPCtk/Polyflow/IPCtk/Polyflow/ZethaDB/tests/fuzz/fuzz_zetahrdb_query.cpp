#include "../../ZethaRDB.hpp"
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string q(reinterpret_cast<const char*>(data), size);
  using namespace zetha::rdb;
  auto schema = schema_dsl::schema("fuzzdb")
      .table("users", {
          schema_dsl::col("id", ValueKind::Text).primary_key(),
          schema_dsl::col("name", ValueKind::Text)
      })
      .build();
  CreateOptions opt{};
  opt.schema_native = schema;
  auto created = ZethaRDB::create("/tmp/zetahrdb_fuzz.zdb", opt);
  if (!created) return 0;
  Row r;
  r["id"] = Value::Text("1");
  r["name"] = Value::Text("x");
  (void)created.value->insert("users", std::move(r));
  (void)created.value->query(q);
  return 0;
}
