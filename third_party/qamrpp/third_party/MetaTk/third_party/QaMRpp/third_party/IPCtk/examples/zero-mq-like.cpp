#include "../IPCtk.hpp"

#include <iostream>
#include <string_view>

int main() {
  constexpr std::string_view ipcl = R"ipcl(
    socket broker = tcp.listen("127.0.0.1:5555");
    queue backlog = mpmc.ring(1024);
    signal has_msg = eventfd();

    pipe ingress = recv(broker) -> decode(zmsg) -> enqueue(backlog) -> notify(has_msg);
    pipe egress = wait(has_msg) -> dequeue(backlog) -> encode(zmsg) -> send(broker);
  )ipcl";

  constexpr std::string_view itkd = R"itkd(
    target cpp
    capability socket
    capability pubsub
    capability signal
    capability codec
    rule recv = zmq.recv(${arg0});
    rule send = zmq.send(${arg0});
    rule enqueue = q.push(${arg0});
    rule dequeue = q.pop(${arg0});
    rule wait = evt.wait(${arg0});
    rule notify = evt.notify(${arg0});
    rule decode = codec.decode(${arg0});
    rule encode = codec.encode(${arg0});
  )itkd";

  auto p = ipctk::ipcl::parse_text(ipcl);
  auto b = ipctk::itkd::parse_text(itkd);
  if (p.is_err() || b.is_err()) {
    std::cerr << "parse failed\n";
    return 1;
  }

  auto emitted = ipctk::compile(p.unwrap(), b.unwrap());
  if (emitted.is_err()) {
    std::cerr << "compile failed\n";
    return 2;
  }

  std::cout << emitted.unwrap();
  return 0;
}
