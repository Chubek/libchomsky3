Chapter 9 - Runtime
====================

Runtime flow:

1. Lookup command in registry.
2. Parse option and positional tokens.
3. Validate required arguments.
4. Apply defaults.
5. Forward parsed state to dispatch layers.

The runtime parser is deterministic and single-pass over argv tokens.
