#include "../IPCtk.hpp"

#include <iostream>
#include <string_view>

int main() {
  constexpr std::string_view ipcl = R"ipcl(
    socket inbox = uds.listen("/tmp/polyflow.in");
    socket outbox = uds.connect("/tmp/polyflow.out");
    queue q = mpmc.ring(256);
    mutex m = semaphore("/polyflow.m", 1);
    signal ready = eventfd();

    pipe ingress = recv(inbox) -> decode(payload) -> lock(m) -> enqueue(q) -> unlock(m) -> notify(ready);
    pipe egress = wait(ready) -> lock(m) -> dequeue(q) -> unlock(m) -> encode(payload) -> send(outbox);
  )ipcl";

  constexpr std::string_view itkd = R"itkd(
    target cpp
    capability socket
    capability pubsub
    capability sync
    capability signal
    capability codec

    rule recv = monitor.emit("recv.start", "${arg0}"); ipc.recv(${arg0});
    rule send = monitor.emit("send.start", "${arg0}"); ipc.send(${arg0});
    rule enqueue = ipc.enqueue(${arg0});
    rule dequeue = ipc.dequeue(${arg0});
    rule lock = ipc.lock(${arg0});
    rule unlock = ipc.unlock(${arg0});
    rule notify = ipc.notify(${arg0});
    rule wait = ipc.wait(${arg0});
    rule decode = ipc.decode(${arg0});
    rule encode = ipc.encode(${arg0});
  )itkd";

  auto program = ipctk::ipcl::parse_text(ipcl);
  auto backend = ipctk::itkd::parse_text(itkd);

  if (program.is_err() || backend.is_err()) {
    std::cerr << "parse failed\n";
    return 1;
  }

  auto out = ipctk::compile(program.unwrap(), backend.unwrap());
  if (out.is_err()) {
    std::cerr << "compile failed\n";
    return 2;
  }

  std::cout << out.unwrap();
  return 0;
}
