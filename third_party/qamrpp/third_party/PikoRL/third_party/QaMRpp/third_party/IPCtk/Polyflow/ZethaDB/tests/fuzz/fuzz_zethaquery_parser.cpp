#include "../../ZethaQUERY.hpp"
#include <string>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data) return 0;
  zetha::query::text::Parser p;
  std::string in(reinterpret_cast<const char*>(data), size);
  auto r = p.parse(in);
  if (r.is_ok()) {
    auto q = r.unwrap();
    (void)q.path.steps.size();
  }
  return 0;
}
