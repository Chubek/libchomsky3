Chapter 3 - Klytmk DSL
=======================

Currently parsed constructs:

- ``param "..." { ... };``
- ``command "..." { ... };``
- ``pre-evaluate { key = "value"; ... };``

Example:

::

  param "b/book="
  {
      help-string
      {
          Book option
      };
  };

  command "build"
  {
  };

  pre-evaluate
  {
      exec = "/bin/bash";
      sanitize = "stdlib/shell.sh";
  };
