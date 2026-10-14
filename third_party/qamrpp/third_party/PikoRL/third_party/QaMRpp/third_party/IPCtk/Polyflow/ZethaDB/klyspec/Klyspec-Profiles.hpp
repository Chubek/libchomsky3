#pragma once

#include "SerdeTk/SerdeTk.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace klyspec {

class KlyProfileLoader {
public:
  enum class Format { json, xml, yaml, sexpr };

  static std::optional<std::unordered_map<std::string, std::string>> load(Format format, const std::string &text) {
    serdetk::Document doc{};
    switch (format) {
      case Format::json: doc = serdetk::builtins::json().load_string(text); break;
      case Format::xml: doc = serdetk::builtins::xml().load_string(text); break;
      case Format::yaml: doc = serdetk::builtins::yaml().load_string(text); break;
      case Format::sexpr: doc = serdetk::builtins::sexpr().load_string(text); break;
    }
    if (!doc.root.is_object()) return std::nullopt;

    std::unordered_map<std::string, std::string> out{};
    for (const auto &entry : doc.root.as_object().fields) {
      if (entry.second.is_string()) {
        out[entry.first] = entry.second.as_string();
      } else if (entry.second.is_int()) {
        out[entry.first] = std::to_string(std::get<std::int64_t>(entry.second.data));
      }
    }
    return out;
  }
};

} // namespace klyspec
