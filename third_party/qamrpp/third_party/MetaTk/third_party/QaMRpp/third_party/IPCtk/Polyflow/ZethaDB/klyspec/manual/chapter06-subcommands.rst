Chapter 6 - Subcommands
========================

Native subcommands implement ``NativeSubcommand`` with:

- ``id()``
- ``name()``
- ``execute(args)``

``SubcommandRegistry`` provides registration, lookup, and dispatch.

Examples are in ``examples/subcommands``:

- build,
- run,
- inspect.
