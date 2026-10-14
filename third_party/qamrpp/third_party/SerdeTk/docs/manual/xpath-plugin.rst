XPath Plugin Manual
===================

Plugin identity
---------------

- Plugin id: ``serdetk.xpath``
- Plugin version: ``3.1``
- Source: ``stdplugin/SerdeTk/XPath``
- Spec alignment: W3C XPath 3.1 (`.agents/standards/XPath-Standard.txt`)

This plugin is implemented for ``serde`` data projected into a uniform
`XPath/XDM`-like sequence model. It is designed to operate on all format
documents loaded through existing adapters (JSON, XML, YAML, S-Expr, BSON,
CBOR, and MessagePack).

Architecture
------------

- Lexer: ``stdplugin/SerdeTk/XPath/Tokenizer``
- AST: ``stdplugin/SerdeTk/XPath/CodeTree``
- Parser: ``stdplugin/SerdeTk/XPath/Parser``
- DAG normalizer: ``stdplugin/SerdeTk/XPath/CodeDAG``
- Validator: ``stdplugin/SerdeTk/XPath/Validator``
- Runtime data model: ``stdplugin/SerdeTk/XPath/DataModel``
- Adapter: ``stdplugin/SerdeTk/XPath/Adapter``
- Evaluator: ``stdplugin/SerdeTk/XPath/Evalutator``
- Plugin façade: ``stdplugin/SerdeTk/XPath/Hooks``

Usage pattern
-------------

::

   #include "Hooks.hpp"

   serdetk::Document document = ...;
   serdetk::xpath::Plugin engine;

   auto result = engine.query(document, "/users/*/name");
   auto diagnostics = engine.validate_expression("count(/users/*)");

`query(document, expression)` returns a ``Sequence`` of matching
``Item`` values. ``validate_expression`` returns a diagnostic set and does
not evaluate.

Supported expression features
----------------------------

- Root and context navigation: ``/``, ``.``
- Child and descendant axes: ``/a/b``
- Wildcards: ``*``
- Descendant wildcard: ``//``
- Attribute axis: ``@name``
- Lookup operator for maps/arrays: ``?``
- Predicates: ``[predicate]`` and position-based filtering
- Unary and binary arithmetic: ``+``, ``-``, ``*``, ``div``, ``idiv``, ``mod``
- Comparisons: ``= != < <= > >=``
- Sequence operators: ``and``, ``or``, ``|``
- Functions: ``boolean``, ``count``, ``empty``, ``exists``, ``false``, ``last``,
  ``name``, ``not``, ``position``, ``string``, ``true``
- Literals: quoted strings, numbers, booleans, ``null``

Limitations
-----------

- XPath 3.1 is implemented as a subset, not a full implementation.
- Sequence constructors cover arrays ``[ ... ]`` and map pairs ``map { k: v }``
  with basic key/value constraints.
- Static and runtime checks map directly to standard error classes where practical
  but not every XPath 3.1 conformance rule is enforced.
- Namespace resolution and advanced axis repertoire are intentionally limited.

Error behavior
--------------

- Invalid tokens and grammar failures produce ``ParseError`` and
  ``err:XPST0003``.
- Function/operator misuse produces ``err:XPST0017`` for unknown
  functions and ``err:XPTY0004`` for type mismatches.
- Empty context for document-root operations raises ``err:XPDY0002``.

Plugin integration with core
---------------------------

The plugin is compiled as ``SerdeTkXPath`` in ``CMakeLists.txt`` and can be linked
with existing SerdeTk binaries or tests.

For non-plugin use, include ``stdplugin/SerdeTk/XPath/Hooks.hpp`` and instantiate
``serdetk::xpath::Plugin``.

The implementation is lightweight and data-agnostic: format-specific behavior is
kept in the underlying document adapter rather than in the evaluator.
