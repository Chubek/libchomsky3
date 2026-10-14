Chapter 15 - SWIG Bindings
==========================

``bindings/PikoRL.i`` is the stable SWIG interface. Keep language-neutral
declarations central and isolate language customization behind SWIG language
conditionals.

Generate wrappers with ``bindings/GenerateBindings.sh``. The driver accepts
one or more ``--lang`` options, an XFeats manifest, output location, and
enabled features. Wrapper compilation remains a consumer build concern.
