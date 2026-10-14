# Podlets User Guide

Podlets are packaged QaMRpp-compatible artifacts distributed as `.qpod`.

## Author a Podlet

Create a directory containing:

- `Podlet.cpp` (required)
- `Podpack.qmr` or `Podpack.lua` (required manifest)
- optional supporting files

Minimal `Podpack.qmr`:

```text
name = hello_podlet
version = 1.0.0
entrypoint = Podlet.cpp
podlet_api = 1
format_version = 1
```

## Build a Podlet

```bash
podpack <podlet_dir> --output hello_podlet.qpod
```

If `--output` is omitted, `podpack` derives a `.qpod` name from the source directory.

## Initialize a new Podlet

```bash
podpack --init my_podlet
```

This scaffolds:

- `my_podlet/Podlet.cpp`
- `my_podlet/Podpack.qmr`

## Install a Podlet

```bash
podpack --install hello_podlet.qpod
```

Default install root:

- `~/.qamrpp/podlet`

Override root:

```bash
podpack --install hello_podlet.qpod --root /custom/path
```

## Load a Podlet in runtime

Use the existing package loading path:

```cpp
qamrpp::Context ctx;
ctx.load_library_named("hello_podlet");
```

Podlet loading is fallback-only and does not replace existing shared-library loading behavior.
