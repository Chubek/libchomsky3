Chapter 7 - IPC
================

IPC integration is provided through ``Klyspec-IPC.hpp`` and ``IPCtk.hpp``.

``KlyIPCService`` exposes:

- ``enable<Mode>(handler)``
- ``serialize(parse_result)``
- ``dispatch(parse_result)``

Supported mode tags:

- ``klyspec::IPC::Signal``
- ``klyspec::IPC::LocalSocket``
