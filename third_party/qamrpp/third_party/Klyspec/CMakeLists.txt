cmake_minimum_required(VERSION 3.16)

project(Klyspec
  VERSION 1.0.0
  DESCRIPTION "Header-only C++20 CLI framework with manifest-driven code generation"
  LANGUAGES CXX)

set(KLYSPEC_VERSION ${PROJECT_VERSION})

# --- Requirements -----------------------------------------------------------
# C++20: the public headers (and the Klytmk parser) use concepts and the
# std::unordered_map::contains API.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# --- Header-only library interface target -----------------------------------
# Public headers live under include/ in the new layout; IPCtk headers (used by
# the IPC and profiles features) are vendored under third_party/ and exposed as
# an include root so references like "IPCtk/IPCtk.hpp" resolve.
add_library(klyspec INTERFACE)
add_library(Klyspec::klyspec ALIAS klyspec)
target_include_directories(klyspec INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(klyspec INTERFACE cxx_std_20)

# --- yaml-cpp (vendored) ----------------------------------------------------
# CLI manifests are YAML-only and parsed via yaml-cpp, which is vendored under
# third_party/yaml-cpp. Build it statically with extras disabled.
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp)
target_link_libraries(klyspec INTERFACE yaml-cpp)

# --- Bootstrapped CLI -------------------------------------------------------
# The klyspec CLI is a single translation unit built from klyspec-cli.cpp,
# which in turn pulls in the generator helpers. It dogfoods the YAML
# manifest pipeline exposed by include/Klyspec.hpp.
add_executable(klyspec-cli cli/klyspec-cli.cpp)
target_link_libraries(klyspec-cli PRIVATE Klyspec::klyspec)

# --- Optional tests ---------------------------------------------------------
option(KLYSPEC_BUILD_TESTS "Build Klyspec smoke/unit tests" OFF)
if(KLYSPEC_BUILD_TESTS)
  enable_testing()
  # Smoke tests cover the migrated public API and the new generators.
  set(KLYSPEC_SMOKE_TESTS
    tests/smoke/smoke_registry.cpp
    tests/smoke/smoke_runtime_parser.cpp
    tests/smoke/smoke_dsl_parser.cpp
    tests/smoke/smoke_profiles.cpp
    tests/smoke/smoke_plugins.cpp
    tests/smoke/smoke_ipc.cpp)
  foreach(test_source IN LISTS KLYSPEC_SMOKE_TESTS)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE Klyspec::klyspec)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
  # Additional manifest-grammar edge-case tests.
  add_executable(test_manifest_edgecases_azmatest tests/unit/test_manifest_edgecases_azmatest.cpp)
  target_link_libraries(test_manifest_edgecases_azmatest PRIVATE Klyspec::klyspec)
  add_test(NAME test_manifest_edgecases_azmatest COMMAND test_manifest_edgecases_azmatest)
endif()

# --- Installation -----------------------------------------------------------
option(KLYSPEC_INSTALL "Install headers, CLI, schema and pkg-config" OFF)
if(KLYSPEC_INSTALL)
  install(TARGETS klyspec-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")

  # Manifest schema ships as a packaged resource under share/klyspec/manifests.
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/manifests/CLIManifest.schema.json
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/klyspec/manifests)

  # pkg-config metadata generated from cmake/Klyspec.pc.in.
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Klyspec.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/Klyspec.pc @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Klyspec.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
