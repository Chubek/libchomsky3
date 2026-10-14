#include "CliKlyspecBridge.hpp"
#include "klyspec/Klyspec.hpp"

#include <string>

bool zetha_cli_is_known(std::string_view cli_name, std::string_view cmd) {
  klyspec::Registry reg;
  auto add = [&](const char* c) {
    klyspec::CommandSpec s;
    s.name = c;
    reg.register_command(std::move(s));
  };
  if (cli_name == "mem") {
    for (const char* c : {"help", "create", "open", "put", "get", "erase", "query", "dump", "load", "close", "exit"}) add(c);
  } else {
    for (const char* c : {"help", "create", "open", "schema-load", "put", "get", "erase", "query", "maintain", "close", "exit"}) add(c);
  }
  return reg.lookup(std::string(cmd)) != nullptr;
}
