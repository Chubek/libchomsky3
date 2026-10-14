Chapter 4 - Feature Modules
===========================

Feature-to-file mapping under ``qamrpp-lpikorl/``:

- prompt -> ``prompt.c``;
- directives -> ``directive.c``;
- syntax -> ``syntax.c``;
- completion -> ``completion.c``;
- help -> ``help.c``;
- history -> ``history.c``;
- idl -> ``idl.c``;
- macros -> ``macros.c``;
- menu -> ``menu.c``;
- page -> ``page.c``;
- sandboxing -> ``sandboxing.c``;
- theme -> ``theme.c``;
- colors -> ``colors.c``;
- web -> ``web.c``.

Shared glue:

- ``lpikorl.h``: shared runtime + declarations;
- ``lpikorl.c``: aggregated binding registration.
