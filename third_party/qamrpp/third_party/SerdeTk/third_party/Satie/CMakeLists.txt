cmake_minimum_required(VERSION 3.16)

project(Satie
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Small, header-only, multi-algorithm SAT solver")

# ------------------------------------------------------------
# Global settings
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Default install locations (overridable via GNUInstallDirs).
include(GNUInstallDirs)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
option(INSTALL_CLI   "Build and install the satie CLI/REPL"            ON)
option(BUILD_TESTING "Build the Catch2 unit-test suite"                ON)
option(BUILD_EXAMPLES "Build the example programs"                     ON)
option(GENERATE_DOCS "Generate Doxygen documentation"                  OFF)

# ------------------------------------------------------------
# Engine library  (namespaced: satie::satie)
# ------------------------------------------------------------
add_library(satie STATIC
  src/Satie.cpp)
add_library(satie::satie ALIAS satie)

target_include_directories(satie
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(satie PUBLIC cxx_std_20)
target_compile_options(satie PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic>)

# Headers are part of the target for IDE/Doxygen discovery.
file(GLOB SATIE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
target_sources(satie PRIVATE ${SATIE_HEADERS})

# ------------------------------------------------------------
# Subdirectories (conditional)
# ------------------------------------------------------------
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(INSTALL_CLI)
  add_subdirectory(cli)
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

add_subdirectory(distrib)
add_subdirectory(docs)

# ------------------------------------------------------------
# Install rules for the library
# ------------------------------------------------------------
install(TARGETS satie
  EXPORT SatieTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")

install(EXPORT SatieTargets
  FILE SatieTargets.cmake
  NAMESPACE satie::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Satie)
