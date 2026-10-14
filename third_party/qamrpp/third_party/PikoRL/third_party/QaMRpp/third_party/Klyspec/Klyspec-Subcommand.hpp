#pragma once

#include "Klyspec.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace klyspec {

class NativeSubcommand {
public:
  virtual ~NativeSubcommand() = default;

  virtual std::string id() const = 0;
  virtual std::string name() const = 0;
  virtual int execute(const std::vector<std::string> &args) = 0;
};

class SubcommandRegistry {
public:
  using DiscoveryHook = std::function<std::vector<std::shared_ptr<NativeSubcommand>>()>;

  bool register_subcommand(std::shared_ptr<NativeSubcommand> subcommand) {
    if (subcommand == nullptr) {
      return false;
    }
    auto by_name = subcommand->name();
    auto by_id = subcommand->id();
    if (by_name.empty() || by_id.empty() || subcommands_by_name_.contains(by_name) || subcommands_by_id_.contains(by_id)) {
      return false;
    }
    subcommands_by_id_.emplace(by_id, subcommand);
    subcommands_by_name_.emplace(std::move(by_name), std::move(subcommand));
    return true;
  }

  std::shared_ptr<NativeSubcommand> lookup(const std::string &name) const {
    auto name_it = subcommands_by_name_.find(name);
    if (name_it != subcommands_by_name_.end()) {
      return name_it->second;
    }
    auto id_it = subcommands_by_id_.find(name);
    return id_it == subcommands_by_id_.end() ? nullptr : id_it->second;
  }

  bool register_discovery_hook(DiscoveryHook hook) {
    if (!hook) {
      return false;
    }
    discovery_hooks_.push_back(std::move(hook));
    return true;
  }

  std::size_t discover() {
    std::size_t discovered = 0;
    for (const auto &hook : discovery_hooks_) {
      for (auto &subcommand : hook()) {
        if (register_subcommand(subcommand)) {
          ++discovered;
        }
      }
    }
    return discovered;
  }

  int dispatch(const std::string &name, const std::vector<std::string> &args) const {
    const auto subcommand = lookup(name);
    if (subcommand == nullptr) {
      return 127;
    }
    return subcommand->execute(args);
  }

private:
  std::unordered_map<std::string, std::shared_ptr<NativeSubcommand>> subcommands_by_name_{};
  std::unordered_map<std::string, std::shared_ptr<NativeSubcommand>> subcommands_by_id_{};
  std::vector<DiscoveryHook> discovery_hooks_{};
};

} // namespace klyspec
