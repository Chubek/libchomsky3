#pragma once

#include "Klyspec.hpp"

#include <memory>
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
  bool register_subcommand(std::shared_ptr<NativeSubcommand> subcommand) {
    if (subcommand == nullptr) {
      return false;
    }
    auto key = subcommand->name();
    if (key.empty() || subcommands_.contains(key)) {
      return false;
    }
    subcommands_.emplace(std::move(key), std::move(subcommand));
    return true;
  }

  std::shared_ptr<NativeSubcommand> lookup(const std::string &name) const {
    auto it = subcommands_.find(name);
    return it == subcommands_.end() ? nullptr : it->second;
  }

  int dispatch(const std::string &name, const std::vector<std::string> &args) const {
    const auto subcommand = lookup(name);
    if (subcommand == nullptr) {
      return 127;
    }
    return subcommand->execute(args);
  }

private:
  std::unordered_map<std::string, std::shared_ptr<NativeSubcommand>> subcommands_{};
};

} // namespace klyspec
