#pragma once

#include "Klyspec.hpp"

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace klyspec {

class PluginHost;

class Plugin {
public:
  virtual ~Plugin() = default;

  virtual std::string name() const = 0;
  virtual void extend_arguments(Registry &) {}
  virtual void before_parse(std::vector<std::string> &) {}
  virtual void after_parse(ParseResult &) {}
  virtual void before_dispatch(const ParseResult &) {}
  virtual std::optional<int> intercept_dispatch(const ParseResult &) { return std::nullopt; }
  virtual void after_dispatch(int) {}
};

template <typename P>
concept Pluggable = std::derived_from<P, Plugin>;

template <Pluggable P>
class KlyPlugin final {
public:
  explicit KlyPlugin(P plugin) : plugin_(std::move(plugin)) {}

  P &get() { return plugin_; }
  const P &get() const { return plugin_; }

private:
  P plugin_;
};

class KlyArgument {
public:
  explicit KlyArgument(ArgumentSpec spec) : spec_(std::move(spec)) {}
  virtual ~KlyArgument() = default;

  const ArgumentSpec &spec() const { return spec_; }

protected:
  ArgumentSpec spec_;
};

class Flag final : public KlyArgument {
public:
  explicit Flag(std::string id, std::vector<std::string> names) : KlyArgument(ArgumentSpec{.id = std::move(id), .kind = ArgumentKind::flag, .value_policy = ValuePolicy::none, .names = std::move(names)}) {}
};

class Switch final : public KlyArgument {
public:
  explicit Switch(std::string id, std::vector<std::string> names) : KlyArgument(ArgumentSpec{.id = std::move(id), .kind = ArgumentKind::switch_, .value_policy = ValuePolicy::optional, .names = std::move(names)}) {}
};

class Option final : public KlyArgument {
public:
  explicit Option(std::string id, std::vector<std::string> names) : KlyArgument(ArgumentSpec{.id = std::move(id), .kind = ArgumentKind::option, .value_policy = ValuePolicy::required, .names = std::move(names)}) {}
};

class PluginRegistry {
public:
  bool register_plugin(std::shared_ptr<Plugin> plugin) {
    if (!plugin) return false;
    const auto plugin_name = plugin->name();
    if (plugin_name.empty() || names_.contains(plugin_name)) {
      return false;
    }
    names_.insert(plugin_name);
    plugins_.push_back(std::move(plugin));
    return true;
  }

  void extend_arguments(Registry &registry) {
    for (const auto &plugin : plugins_) plugin->extend_arguments(registry);
  }

  void before_parse(std::vector<std::string> &argv) {
    for (const auto &plugin : plugins_) plugin->before_parse(argv);
  }

  void after_parse(ParseResult &result) {
    for (const auto &plugin : plugins_) plugin->after_parse(result);
  }

  void before_dispatch(const ParseResult &result) {
    for (const auto &plugin : plugins_) plugin->before_dispatch(result);
  }

  std::optional<int> intercept_dispatch(const ParseResult &result) {
    for (const auto &plugin : plugins_) {
      if (auto code = plugin->intercept_dispatch(result); code.has_value()) {
        return code;
      }
    }
    return std::nullopt;
  }

  void after_dispatch(int code) {
    for (const auto &plugin : plugins_) plugin->after_dispatch(code);
  }

private:
  std::vector<std::shared_ptr<Plugin>> plugins_{};
  std::unordered_set<std::string> names_{};
};

} // namespace klyspec
