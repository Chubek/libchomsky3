#include "../../ZethaSTORAGE.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

static int failures = 0;
static void expect(bool cond, const char* msg) { if (!cond) { std::cerr << msg << "\n"; ++failures; } }

int main() {
  namespace fs = std::filesystem;
  const fs::path db_path = fs::temp_directory_path() / "zethadb_stage5_storage_test.zdb";
  std::error_code ec;
  fs::remove(db_path, ec);

  zetha::storage::CreateOptions opt;
  opt.schema.json_schema = R"({"type":"object"})";
  auto created = zetha::storage::ZethaStorage::create(db_path.string(), opt);
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(created), "create should succeed");
  if (!std::holds_alternative<zetha::storage::ZethaStorage>(created)) return EXIT_FAILURE;
  auto db = std::move(std::get<zetha::storage::ZethaStorage>(created));

  auto put = db.insert("users/1", "{\"name\":\"n\"}");
  expect(!put.has_value(), "insert should succeed");

  auto get1 = db.get("users/1");
  expect(std::holds_alternative<std::optional<std::string>>(get1), "get should return expected variant");
  if (std::holds_alternative<std::optional<std::string>>(get1)) {
    const auto& v = std::get<std::optional<std::string>>(get1);
    expect(v.has_value(), "inserted key should exist");
    if (v.has_value()) expect(*v == "{\"name\":\"n\"}", "value should match inserted payload");
  }

  auto erase = db.erase("users/1");
  expect(!erase.has_value(), "erase should succeed");
  auto get2 = db.get("users/1");
  expect(std::holds_alternative<std::optional<std::string>>(get2), "get after erase should return expected variant");
  if (std::holds_alternative<std::optional<std::string>>(get2)) {
    expect(!std::get<std::optional<std::string>>(get2).has_value(), "erased key should be missing");
  }

  auto put2 = db.insert("users/2", "v2");
  expect(!put2.has_value(), "second insert should succeed");
  auto put3 = db.insert("users/0", "v0");
  expect(!put3.has_value(), "third insert should succeed");
  auto defrag = db.defragment();
  expect(!defrag.has_value(), "defragment should succeed");

  std::string scan_joined;
  for (auto it = db.scan_begin(); it.valid(); it.next()) {
    auto item = it.item();
    scan_joined += std::string(item.key) + "=" + std::string(item.value) + ";";
  }
  expect(scan_joined == "users/0=v0;users/2=v2;", "scan iterator should be stable and ordered");

  auto closed = db.close();
  expect(!closed.has_value(), "close should succeed");

  auto reopened = zetha::storage::ZethaStorage::open(db_path.string(), zetha::storage::OpenOptions{});
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(reopened), "open should succeed");
  if (std::holds_alternative<zetha::storage::ZethaStorage>(reopened)) {
    auto db2 = std::move(std::get<zetha::storage::ZethaStorage>(reopened));
    auto get3 = db2.get("users/2");
    expect(std::holds_alternative<std::optional<std::string>>(get3), "reopen get should return expected variant");
    if (std::holds_alternative<std::optional<std::string>>(get3)) {
      const auto& v = std::get<std::optional<std::string>>(get3);
      expect(v.has_value(), "persisted key should exist");
      if (v.has_value()) expect(*v == "v2", "persisted value should match");
    }
    auto snap = db2.scan_snapshot_from_journal();
    expect(std::holds_alternative<std::vector<std::pair<std::string, std::string>>>(snap), "journal snapshot scan should succeed");
    if (std::holds_alternative<std::vector<std::pair<std::string, std::string>>>(snap)) {
      const auto& rows = std::get<std::vector<std::pair<std::string, std::string>>>(snap);
      expect(rows.size() == 2, "journal snapshot should contain two rows");
      if (rows.size() == 2) {
        expect(rows[0].first == "users/0" && rows[0].second == "v0", "snapshot row0 should match");
        expect(rows[1].first == "users/2" && rows[1].second == "v2", "snapshot row1 should match");
      }
    }
  }

  const fs::path bad_op_path = fs::temp_directory_path() / "zethadb_stage5_badop.zdb";
  fs::remove(bad_op_path, ec);
  auto bad_created = zetha::storage::ZethaStorage::create(bad_op_path.string(), opt);
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(bad_created), "bad-op create should succeed");
  if (std::holds_alternative<zetha::storage::ZethaStorage>(bad_created)) {
    auto bad_db = std::move(std::get<zetha::storage::ZethaStorage>(bad_created));
    expect(!bad_db.insert("k", "v").has_value(), "bad-op seed insert should succeed");
    const auto h = bad_db.header_fixed();
    std::fstream f(bad_op_path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(static_cast<std::streamoff>(h.journal_begin), std::ios::beg);
    const unsigned char op = 255;
    f.write(reinterpret_cast<const char*>(&op), 1);
  }
  auto bad_open = zetha::storage::ZethaStorage::open(bad_op_path.string(), zetha::storage::OpenOptions{});
  expect(std::holds_alternative<zetha::storage::Error>(bad_open), "open should fail on unknown journal op");
  if (std::holds_alternative<zetha::storage::Error>(bad_open)) {
    expect(std::get<zetha::storage::Error>(bad_open).code == zetha::storage::Errc::Corrupt, "unknown op should report corrupt");
  }

  const fs::path bad_bounds_path = fs::temp_directory_path() / "zethadb_stage5_badbounds.zdb";
  fs::remove(bad_bounds_path, ec);
  auto bounds_created = zetha::storage::ZethaStorage::create(bad_bounds_path.string(), opt);
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(bounds_created), "bad-bounds create should succeed");
  if (std::holds_alternative<zetha::storage::ZethaStorage>(bounds_created)) {
    auto bad_db = std::move(std::get<zetha::storage::ZethaStorage>(bounds_created));
    expect(!bad_db.insert("k", "v").has_value(), "bad-bounds seed insert should succeed");
    const auto h = bad_db.header_fixed();
    std::fstream f(bad_bounds_path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(static_cast<std::streamoff>(h.journal_begin + 1), std::ios::beg);
    const std::uint32_t bad_klen = 0xFFFFFFFFu;
    f.write(reinterpret_cast<const char*>(&bad_klen), sizeof(bad_klen));
  }
  auto bounds_open = zetha::storage::ZethaStorage::open(bad_bounds_path.string(), zetha::storage::OpenOptions{});
  expect(std::holds_alternative<zetha::storage::Error>(bounds_open), "open should fail on journal bounds corruption");
  if (std::holds_alternative<zetha::storage::Error>(bounds_open)) {
    expect(std::get<zetha::storage::Error>(bounds_open).code == zetha::storage::Errc::Corrupt, "bounds corruption should report corrupt");
  }

  const fs::path mmap_path = fs::temp_directory_path() / "zethadb_stage6_mmap.bin";
  {
    std::ofstream mf(mmap_path, std::ios::binary | std::ios::trunc);
    std::string blob(8192, '\0');
    mf.write(blob.data(), static_cast<std::streamsize>(blob.size()));
  }
  zetha::storage::MMapWindow mw;
  auto m1 = mw.map_readwrite(mmap_path.string(), 1, 4096, 4096);
  expect(std::holds_alternative<zetha::storage::Error>(m1), "unaligned mmap offset should fail");
  auto m2 = mw.map_readwrite(mmap_path.string(), 0, 123, 4096);
  expect(std::holds_alternative<zetha::storage::Error>(m2), "unaligned mmap length should fail");
  auto m3 = zetha::storage::MMapWindow::validate_page_window(4096, 4096, 4096);
  expect(!m3.has_value(), "aligned mmap window should validate");

  const fs::path crash_path = fs::temp_directory_path() / "zethadb_stage7_crash_tail.zdb";
  fs::remove(crash_path, ec);
  auto crash_created = zetha::storage::ZethaStorage::create(crash_path.string(), opt);
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(crash_created), "crash create should succeed");
  if (std::holds_alternative<zetha::storage::ZethaStorage>(crash_created)) {
    auto crash_db = std::move(std::get<zetha::storage::ZethaStorage>(crash_created));
    expect(!crash_db.insert("a", "1").has_value(), "crash seed insert should succeed");
    expect(!crash_db.insert("b", "2").has_value(), "crash seed insert 2 should succeed");
    auto hdr = crash_db.header_fixed();
    std::error_code tec;
    fs::resize_file(crash_path, hdr.journal_end - 4, tec); // simulate crash in trailing record
    expect(!tec, "crash tail truncate should succeed");
  }
  auto crash_open = zetha::storage::ZethaStorage::open(crash_path.string(), zetha::storage::OpenOptions{});
  expect(std::holds_alternative<zetha::storage::ZethaStorage>(crash_open), "open should tolerate crash tail");
  if (std::holds_alternative<zetha::storage::ZethaStorage>(crash_open)) {
    auto crash_db2 = std::move(std::get<zetha::storage::ZethaStorage>(crash_open));
    auto ga = crash_db2.get("a");
    auto gb = crash_db2.get("b");
    expect(std::holds_alternative<std::optional<std::string>>(ga), "crash get a should succeed");
    expect(std::holds_alternative<std::optional<std::string>>(gb), "crash get b should succeed");
    if (std::holds_alternative<std::optional<std::string>>(ga)) {
      expect(std::get<std::optional<std::string>>(ga).has_value(), "committed key a should persist");
    }
    if (std::holds_alternative<std::optional<std::string>>(gb)) {
      expect(!std::get<std::optional<std::string>>(gb).has_value(), "partial trailing txn should be dropped");
    }
  }

  fs::remove(db_path, ec);
  fs::remove(bad_op_path, ec);
  fs::remove(bad_bounds_path, ec);
  fs::remove(mmap_path, ec);
  fs::remove(crash_path, ec);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
