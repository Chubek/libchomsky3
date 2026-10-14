# Examples

Examples are runnable authoring references, not installable runtime artifacts.

- `PythonRL` and `RubyRL` demonstrate manifest-driven Lua bundles;
- `bundle/` demonstrates packaging and entrypoint dispatch;
- `plugins/` demonstrates the public native plugin ABI.

The native plugin example requires:

```sh
c++ -std=c++17 -Iinclude -fPIC -shared \
  examples/plugins/hello-plugin.cpp -o hello-plugin.so
```
