Chapter 12 - Embedded QaMRpp
============================

QaMRpp is a hosted scripting runtime. Its integration layer belongs in
``interop/QaMRpp`` and exposes controlled operations: evaluation, command
registration, prompts, syntax hooks, and completion hooks.

QaMRpp scripts can customize a running host but do not gain addon packaging or
native-plugin ABI status. Use an addon manifest for distributable packages and
``PikoRL-Plugin.hpp`` for compiled modules.
