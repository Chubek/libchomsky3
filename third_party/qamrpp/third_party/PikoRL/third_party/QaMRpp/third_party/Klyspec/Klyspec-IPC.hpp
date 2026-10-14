#pragma once

#include "IPCtk/IPCtk.hpp"
#include "Klyspec.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace klyspec {
namespace IPC {
class Signal {
public:
  static constexpr std::string_view name() { return "signal"; }
};
class LocalSocket {
public:
  static constexpr std::string_view name() { return "local_socket"; }
};
template <typename Backend>
class Adapter {
public:
  using backend_type = Backend;
  static constexpr std::string_view name() { return "adapter"; }
};
} // namespace IPC

class KlyIPCService {
public:
  template <typename Mode, typename Handler>
  void enable(Handler handler) {
    mode_name_ = std::string(Mode::name());
    handler_ = [h = std::move(handler)](const ParseResult &r) { return h(r); };
  }

  std::optional<int> dispatch(const ParseResult &parsed) const {
    if (!handler_) return std::nullopt;
    return handler_(parsed);
  }

  auto serialize(const ParseResult &parsed) const -> ipctk::Result<std::vector<std::uint8_t>> {
    if (mode_name_.empty()) {
      return ipctk::Result<std::vector<std::uint8_t>>::from_err(
          ipctk::make_error(ipctk::Error::Code::Validation, "ipc mode is not enabled"));
    }
    // Stable key ordering keeps messages deterministic for tests/logging.
    std::map<std::string, std::vector<std::string>> ordered(parsed.values.begin(), parsed.values.end());
    std::string out = "mode=" + mode_name_ + ";ok=" + std::string(parsed.ok ? "1" : "0") + ";";
    for (const auto &kv : ordered) {
      out += kv.first + "=";
      for (std::size_t i = 0; i < kv.second.size(); ++i) {
        if (i != 0) out += ",";
        out += kv.second[i];
      }
      out += ";";
    }
    if (!parsed.positionals.empty()) {
      out += "positionals=";
      for (std::size_t i = 0; i < parsed.positionals.size(); ++i) {
        if (i != 0) out += ",";
        out += parsed.positionals[i];
      }
      out += ";";
    }
    return ipctk::Result<std::vector<std::uint8_t>>::from_ok(std::vector<std::uint8_t>(out.begin(), out.end()));
  }

private:
  std::string mode_name_{};
  std::function<int(const ParseResult &)> handler_{};
};

} // namespace klyspec
