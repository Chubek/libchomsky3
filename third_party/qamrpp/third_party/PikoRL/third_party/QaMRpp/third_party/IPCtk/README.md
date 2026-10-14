# IPCtk: Header-Only, Minimal IPC Library with Native + Syntax-Based DSL

IPCtk is a header-only C++ library that provides a toolkit for inter-process communication. IPCtk is minimal, garbage-free and it offers a C++-native DSL, plus a syntax-based DSL, using the DSLUtils.hpp header-only library. 


The purpose of this DSL is to design high-level IPC protocols, like:
- Pub/Sub
- Req/Rep
- Surveyor/Respondnet
- Push/Pull
- Message Bus
- Event Bus
- RPC
- Mailboxes
- Channels

You can create all these using what IPC-L (the name for both the native and syntax-based DSL) offers. IPC-L offers not only raw IPC protocols:
- Pipes
- FIFOs
- Shared Memory
- Message Queues
- TCP/UDP Sockets
- Unix Domain Sockets
- Memory-mapped files
- Semaphores
- Signals
- Futexes (Only on Linux)
- Eventfd (Only on Linux)
- io_ring (Only on Linux)

But it offers ways to *compose* them to achieve high-level protocols, like those we mentioned above. For example, to create a pub/sub protocol in IPC-L, we simply do:

```ipcl
socket pub_in        = tcp.listen("127.0.0.1:7000");
socket sub_in        = tcp.listen("127.0.0.1:7001");

shared subscription_table = shm.open("/subscriptions", 128_KiB);
mutex  subscription_lock  = semaphore("/subscriptions.lock", 1);

queue publication_queue   = mpmc.ring(4096);
signal publication_ready  = eventfd();

pipe publish_path =
    recv(pub_in)
    -> decode(message)
    -> enqueue(publication_queue)
    -> notify(publication_ready);

pipe subscribe_path =
    recv(sub_in)
    -> decode(subscription)
    -> lock(subscription_lock)
    -> update(subscription_table)
    -> unlock(subscription_lock);

pipe dispatch_path =
    wait(publication_ready)
    -> dequeue(publication_queue)
    -> lock(subscription_lock)
    -> match_subscribers(subscription_table)
    -> unlock(subscription_lock)
    -> fanout(send(sub_in));
```

The C++-native DSL version of this would be:

```cpp
using namespace ipctk;
using namespace ipctk::dsl;

auto pub_in = socket("pub_in") = tcp.listen("127.0.0.1:7000");
auto sub_in = socket("sub_in") = tcp.listen("127.0.0.1:7001");

auto subscription_table = shared("subscription_table") = shm.open("/subscriptions", 128_KiB);
auto subscription_lock  = mutex("subscription_lock")   = semaphore("/subscriptions.lock", 1);

auto publication_queue  = queue("publication_queue")   = mpmc.ring(4096);
auto publication_ready  = signal("publication_ready")  = eventfd();

auto publish_path =
    pipe("publish_path") =
        recv(pub_in)
        >> decode(as<message>)
        >> enqueue(publication_queue)
        >> notify(publication_ready);

auto subscribe_path =
    pipe("subscribe_path") =
        recv(sub_in)
        >> decode(as<subscription>)
        >> lock(subscription_lock)
        >> update(subscription_table)
        >> unlock(subscription_lock);

auto dispatch_path =
    pipe("dispatch_path") =
        wait(publication_ready)
        >> dequeue(publication_queue)
        >> lock(subscription_lock)
        >> match_subscribers(subscription_table)
        >> unlock(subscription_lock)
        >> fanout(send(sub_in));
```

## Compiling Syntactic DSL Files

Since IPCtk.hpp is a header-only library, it is not possible to compile it. So to build a DSL file, you simply do this:

```cpp
#include <ipctk.hpp>

auto pubsub = ipctk::syn::parse("pubsub.ipcl");
pubsub.compile<"dest/Python.itkd">("pubsub.py");
```

## The ITKD Language

Using the ITKD language, you can define new backends for IPC-L targets. By default, several backends have been provided in `dest` directory. These are C, Python and Ruby.

In the `examples` directory, several imaginary backends have been defiend to demonstrate the usage of ITKD. Inside the `docs` directory, you can find the manual for both IPC-L and ITKD.

## Circular Dependency

IPCtk depends on Polyflow, and Polyflow depends on IPCtk. How does that work?

It's a rather clumsy issue. The root directory is mirrored in `Polyflow/IPCtk`. But any simple SAT solver would tell you, since the IPCtk API is decoupled from Polyflow API, this creates a situation where IPCtk and Polyflow live togethe r in a 'co-independent' state. They are exposed to each other through each other's APIs, and their APIs are entirely decoupled.

About the mirroring, there's little I can do about it, until I make my own package manager.

Please visit my [SAT solver](https://github.com/Chubek/Satie) if you are interested in solving this problem yourself. I did not use a SAT solver, I used common sense.

## Bindings

We use SWIG 4.0 to generate bindings for the languages it supports. However, you may notice `XFeats.yaml`. This is my dataset of "language-specific capabilities". Through `GenerateBindings.sh`, you can enable them. Keep in mind that some of them collide. Use common sense. The easiest way to generate bindings is:

```sh
$ bash bindings/GenerateBindings.sh Python
```
