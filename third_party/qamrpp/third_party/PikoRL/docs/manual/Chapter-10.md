Chapter 10 - Extension Manifest
================================

``manifests/Extensions.yaml`` is authoritative extension metadata. Each entry
declares a stable name, an implementation path, and authoring semantics.

Extensions remain runtime capabilities. Packaging an extension with user assets
creates an addon; compiling a loadable shared object creates a plugin. Do not
derive install destinations from manifest paths. Addons install to ``addons``;
standard assets install to ``stdlib``.
