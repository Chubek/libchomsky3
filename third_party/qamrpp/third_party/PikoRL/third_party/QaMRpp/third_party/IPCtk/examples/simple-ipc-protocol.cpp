#include "../IPCtk.hpp"

#include <iostream>
#include <string_view>

int main() {
  constexpr std::string_view ipcl = R"ipcl(
    socket client = uds.connect("/tmp/simple.sock");
    pipe protocol = recv(client) -> decode(message) -> encode(message) -> send(client);
  )ipcl";

  constexpr std::string_view itkd = R"itkd(
    target cpp
    capability socket
    capability codec
    rule recv = ipc.recv(${arg0});
    rule send = ipc.send(${arg0});
    rule decode = ipc.decode(${arg0});
    rule encode = ipc.encode(${arg0});
  )itkd";

  auto p = ipctk::ipcl::parse_text(ipcl);
  if (p.is_err()) {
    std::cerr << "ipcl parse failed\n";
    return 1;
  }

  auto b = ipctk::itkd::parse_text(itkd);
  if (b.is_err()) {
    std::cerr << "itkd parse failed\n";
    return 1;
  }

  auto out = ipctk::compile(p.unwrap(), b.unwrap());
  if (out.is_err()) {
    std::cerr << "compile failed\n";
    return 2;
  }

  std::cout << out.unwrap();
  return 0;
}
