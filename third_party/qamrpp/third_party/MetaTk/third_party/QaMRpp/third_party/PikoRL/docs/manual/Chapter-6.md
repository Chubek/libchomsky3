Chapter 6 - Build Integration
=============================

CMake integration:

- root ``CMakeLists.txt`` includes ``qamrpp-lpikorl`` and optional ``docs``;
- ``qamrpp-lpikorl/CMakeLists.txt`` builds feature sources with QaMRpp C library;
- ``docs/CMakeLists.txt`` configures ``Doxyfile`` and defines ``docs`` target.

Critical requirements:

- keep subdirectory inclusion from root;
- keep QaMRpp library integration model in feature build;
- avoid disconnected build logic.
