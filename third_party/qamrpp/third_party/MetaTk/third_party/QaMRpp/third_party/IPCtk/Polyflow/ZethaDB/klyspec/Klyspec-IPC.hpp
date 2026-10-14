#pragma once

#include "IPCtk/IPCtk.hpp"
#include "Klyspec.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace klyspec {
namespace IPC {
class Signal {};
class LocalSocket {};
template <typename Backend>
class Adapter {};
} // namespace IPC

class KlyIPCService {
public:
  template <typename Mode, typename Handler>
  void enable(Handler handler) {
    mode_name_ = typeid(Mode).name();
    handler_ = [h = std::move(handler)](const ParseResult &r) { return h(r); };
  }

  std::optional<int> dispatch(const ParseResult &parsed) const {
    if (!handler_) return std::nullopt;
    return handler_(parsed);
  }

  auto serialize(const ParseResult &parsed) const -> ipctk::Result<std::vector<std::uint8_t>> {
    std::string out = "args:";
    for (const auto &kv : parsed.values) {
      out += kv.first + "=";
      if (!kv.second.empty()) out += kv.second.front();
      out += ";";
    }
    return std::vector<std::uint8_t>(out.begin(), out.end());
  }

private:
  std::string mode_name_{};
  std::function<int(const ParseResult &)> handler_{};
};

} // namespace klyspec
