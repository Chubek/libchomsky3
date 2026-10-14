#pragma once

#include "SerdeTk/SerdeTk.hpp"

#include <cstdint>
#include <functional>
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
    std::function<void(const std::string &, const serdetk::Value &)> flatten;
    flatten = [&](const std::string &prefix, const serdetk::Value &node) {
      if (node.is_string()) {
        out[prefix] = node.as_string();
      } else if (node.is_int()) {
        out[prefix] = std::to_string(std::get<std::int64_t>(node.data));
      } else if (node.is_double()) {
        out[prefix] = std::to_string(std::get<double>(node.data));
      } else if (node.is_bool()) {
        out[prefix] = std::get<bool>(node.data) ? "true" : "false";
      } else if (node.is_object()) {
        for (const auto &entry : node.as_object().fields) {
          const auto key = prefix.empty() ? entry.first : (prefix + "." + entry.first);
          flatten(key, entry.second);
        }
      }
    };
    for (const auto &entry : doc.root.as_object().fields) {
      flatten(entry.first, entry.second);
    }
    return out;
  }
};

} // namespace klyspec
