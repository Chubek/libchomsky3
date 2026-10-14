Chapter 3 - Public API
======================

Primary public concepts:

- ``picorl::PicoRLExtension``;
- ``picorl::REPL``.

``picorl::PicoRLExtension`` responsibilities:

- provide extension name;
- register core natives used by bootstrap.

``picorl::REPL`` responsibilities:

- initialize QaMRpp stdlib;
- install extension;
- bootstrap ``lpicorl`` table;
- run REPL loop;
- load example bundles via manifest.
