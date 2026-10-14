# S7 Scheme Interop

S7 Scheme is an embedded scripting runtime.

- isolate Scheme host bindings in this directory;
- expose evaluation and explicit registration hooks;
- retain core ownership of runtime lifecycle and host state;
- scripts are not automatically addons or native plugins.
