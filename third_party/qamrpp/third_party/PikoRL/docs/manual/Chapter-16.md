Chapter 16 - XFeats
===================

``bindings/XFeats.yaml`` declares xfeature identifiers and supported target
languages. The generator validates every requested feature before invoking
SWIG; unsupported and unknown features are fatal.

Example:

```
bindings/GenerateBindings.sh --lang python --enable-xfeats +py-docstrings
```

Feature validation does not synthesize typemaps. An xfeature requires matching
conditional implementation in ``PikoRL.i`` before it is advertised as active.
