# Hello Podlet

Minimal example Podlet project.

Files:

- `Podlet.cpp`: Podlet source payload
- `Podpack.qmr`: required Podlet manifest

Quick build/install/load flow:

```bash
./build/cli/podpack examples/podlets/hello_podlet --output /tmp/hello_podlet.qpod
./build/cli/podpack --install /tmp/hello_podlet.qpod
```

Runtime load (from QaMRpp host code):

```cpp
qamrpp::Context ctx;
ctx.load_library_named("hello_podlet");
```
