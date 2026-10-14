Chapter 5 - Example Bundles and Manifest
=========================================

Supported example shapes:

- ``examples/PythonRL``;
- ``examples/RubyRL``.

Manifest rules:

- read ``MANIFEST.json`` via SerdeTk;
- treat manifest entries as authoritative;
- allow partial feature bundles;
- skip missing optional files;
- fail only on missing required resources.

Lua resource loading:

- load Lua files through QaMRpp runtime execution;
- keep feature loading modular.

Bundle scaffolding (``pikorl-cli``):

- run ``pikorl-cli <bundle-dir>`` to create a bundle skeleton;
- optional ``--stub <path>`` injects a custom runnable-stub script;
- generated files include Lua feature hooks, ``MANIFEST.json``, ``entrypoint.sh``, ``package.sh``, ``code/``, and ``static/``.

Runnable archive model:

- ``package.sh`` builds a tar payload (prefer vendored libarchive ``bsdtar`` when present);
- output archive is ``stub + tar payload``;
- stub extracts payload in a temporary runtime directory and dispatches:
  - ``entrypoint.sh`` first;
  - then ``entrypoint.py``;
  - then first file matching ``entrypoint*``.
