Chapter 10 - Security
======================

Current baseline protections:

- deterministic parsing,
- no dynamic eval in parser runtime,
- explicit pre-evaluate block parsing (data only).

Planned hardening targets:

- command execution sanitization policy,
- stricter pre-evaluation allowlists,
- IPC payload validation before dispatch.
