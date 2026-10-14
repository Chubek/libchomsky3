#include "../../ZethaMEM.hpp"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using zethamem::memv2::MemDB;
  using zethamem::memv2::Mode;
  using zethamem::memv2::Record;

  MemDB db(Mode::PureRAM);
  Record r;
  r.fields["id"] = std::int64_t{1};
  r.fields["status"] = std::string{"seed"};
  (void)db.put("users/items", r);

  std::string in(reinterpret_cast<const char*>(data), size);
  (void)db.query(in);
  return 0;
}
