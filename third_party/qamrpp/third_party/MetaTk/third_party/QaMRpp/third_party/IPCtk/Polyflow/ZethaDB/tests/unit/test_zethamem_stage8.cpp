#include "../../ZethaMEM.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

static int failures = 0;
static void expect(bool cond, const char* msg) { if (!cond) { std::cerr << msg << "\n"; ++failures; } }

int main() {
  using zethamem::memv2::MemDB;
  using zethamem::memv2::Mode;
  using zethamem::memv2::Record;

  MemDB ram(Mode::PureRAM);
  Record r1;
  r1.fields["id"] = std::int64_t{42};
  r1.fields["status"] = std::string{"open"};
  expect(ram.put("users/orders", r1).is_ok(), "ram put should succeed");
  auto q1 = ram.query("/users/orders[@id=42][@status='open']");
  expect(q1.is_ok(), "query parse/eval should succeed");
  if (q1.is_ok()) expect(q1.unwrap().size() == 1, "query should match one row");

  auto q2 = ram.query("/users/orders[@id=99]");
  expect(q2.is_ok(), "negative query should still parse");
  if (q2.is_ok()) expect(q2.unwrap().empty(), "negative query should return empty");

  auto erased = ram.erase("users/orders");
  expect(erased.is_ok(), "erase should succeed");
  auto got = ram.get("users/orders");
  expect(got.is_ok(), "get after erase should succeed");
  if (got.is_ok()) expect(!got.unwrap().has_value(), "erased key should not exist");

  namespace fs = std::filesystem;
  const auto dump_path = (fs::temp_directory_path() / "zethamem_stage8_dump.zdb").string();
  Record r2;
  r2.fields["id"] = std::int64_t{7};
  r2.fields["status"] = std::string{"new"};
  expect(ram.put("users/tickets", r2).is_ok(), "put before dump should succeed");
  expect(ram.dump_to_file(dump_path).is_ok(), "dump_to_file should succeed");

  MemDB ram2(Mode::PureRAM);
  expect(ram2.load_from_file(dump_path).is_ok(), "load_from_file should succeed");
  auto q3 = ram2.query("/users/tickets[@status='new']");
  expect(q3.is_ok(), "loaded db query should parse");
  if (q3.is_ok()) expect(q3.unwrap().size() == 1, "loaded db query should match");

  const auto mmap_path = (fs::temp_directory_path() / "zethamem_stage8_mmap.zdb").string();
  MemDB mapped;
  expect(mapped.open_mmap(mmap_path, 1 << 20).is_ok(), "open_mmap should succeed");
  Record rm;
  rm.fields["id"] = std::int64_t{1};
  rm.fields["status"] = std::string{"hot"};
  expect(mapped.put("users/tasks", rm).is_ok(), "mmap put should succeed");

  MemDB mapped2;
  expect(mapped2.open_mmap(mmap_path, 1 << 20).is_ok(), "reopen mmap should succeed");
  auto q4 = mapped2.query("/users/tasks[@status='hot']");
  expect(q4.is_ok(), "reopened mmap query should parse");
  if (q4.is_ok()) expect(q4.unwrap().size() == 1, "reopened mmap query should match");

  std::error_code ec;
  fs::remove(dump_path, ec);
  fs::remove(mmap_path, ec);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
