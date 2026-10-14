Chapter 2 - Architecture
========================

Klyspec data flow:

::

  Klytmk text
      |
      v
  DSLUtils combinators
      |
      v
  Klytmk AST + diagnostics
      |
      v
  Registry
      |
      v
  Runtime parser
      |
      v
  Dispatch + plugins + subcommands + IPC

Core responsibilities:

- ``Registry`` owns command specs and alias maps.
- ``KlyCLIService`` parses argv deterministically.
- Plugins observe and modify parse/dispatch lifecycle.
- Subcommand registry maps names to native command handlers.
- IPC service serializes parsed arguments for remote handoff.
