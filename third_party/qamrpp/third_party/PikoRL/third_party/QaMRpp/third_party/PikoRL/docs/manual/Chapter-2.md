Chapter 2 - Runtime and Data Roles
===================================

QaMRpp role:

- runtime execution;
- Lua-facing function registration;
- extension integration;
- native binding dispatch.

SerdeTk role:

- structured data parsing;
- ``MANIFEST.json`` loading;
- future JSON/XML/YAML/S-Expr resource parsing.

Layer boundary:

- do not parse manifest JSON in ad hoc code paths;
- do not replace QaMRpp runtime registration with alternate stacks.
