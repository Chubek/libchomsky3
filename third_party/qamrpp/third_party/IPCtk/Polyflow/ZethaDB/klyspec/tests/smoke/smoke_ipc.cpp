#include "Klyspec-IPC.hpp"

#include <cassert>

int main() {
  klyspec::ParseResult parsed;
  parsed.ok = true;
  parsed.values["file"].push_back("x.txt");

  klyspec::KlyIPCService ipc;
  ipc.enable<klyspec::IPC::Signal>([](const klyspec::ParseResult &r) { return r.ok ? 0 : 1; });
  auto ser = ipc.serialize(parsed);
  assert(ser.is_ok());
  auto rc = ipc.dispatch(parsed);
  assert(rc && *rc == 0);
  return 0;
}
