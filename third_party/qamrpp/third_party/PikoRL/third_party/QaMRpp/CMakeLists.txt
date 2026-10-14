cmake_minimum_required(VERSION 3.16)
project(QaMRppStdLib VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
enable_testing()

option(BUILD_CLI "Build qamrpp CLI tools" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED QAMRPP_HOME)
  if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(QAMRPP_HOME_DEFAULT "$ENV{HOME}/.qamrpp")
  else()
    set(QAMRPP_HOME_DEFAULT ".qamrpp")
  endif()
  set(QAMRPP_HOME "${QAMRPP_HOME_DEFAULT}" CACHE PATH "Home directory used for local QaMRpp assets.")
endif()
if(QAMRPP_HOME MATCHES "^~")
  if(NOT DEFINED ENV{HOME} OR "$ENV{HOME}" STREQUAL "")
    message(FATAL_ERROR "HOME is not set; cannot expand ~ in QAMRPP_HOME")
  endif()
  string(REGEX REPLACE "^~" "$ENV{HOME}" QAMRPP_HOME "${QAMRPP_HOME}")
endif()
if(NOT DEFINED QAMRPP_HOME OR QAMRPP_HOME STREQUAL "")
  message(FATAL_ERROR "QAMRPP_HOME must be a valid non-empty path")
endif()

set(QAMRPP_PKGCONFIG_VERSION "${PROJECT_VERSION}")
message(STATUS "QAMRPP_HOME: ${QAMRPP_HOME}")

add_library(qamrpp_library OBJECT lib/QaMRpp-Library.c)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/qamrpp.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/qamrpp.pc
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/qamrpp.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

option(MAKE_DOCS "Build API documentation with Doxygen" ON)

if(MAKE_DOCS)
  find_program(DOXYGEN_EXECUTABLE doxygen)
  if(NOT DOXYGEN_EXECUTABLE)
    message(FATAL_ERROR "MAKE_DOCS=ON but doxygen is required and was not found")
  endif()

  set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")
  configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
      ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      @ONLY
  )

  add_custom_target(docs ALL
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
  )

  set(_QAMRPP_DOCS_INSTALL_DEST "${CMAKE_INSTALL_DOCDIR}/qamrpp")
  add_custom_target(docs-install DEPENDS docs)

  install(
      DIRECTORY ${DOXYGEN_OUTPUT_DIR}/html/
      DESTINATION ${_QAMRPP_DOCS_INSTALL_DEST}/html
      COMPONENT documentation
  )

  install(
      DIRECTORY ${DOXYGEN_OUTPUT_DIR}/latex/
      DESTINATION ${_QAMRPP_DOCS_INSTALL_DEST}/latex
      COMPONENT documentation
  )

  if(TARGET install)
      add_dependencies(install docs-install)
  endif()
endif()

add_subdirectory(stdlib)
add_subdirectory(stdlib++)
add_subdirectory(plugins)

if(BUILD_CLI)
  add_subdirectory(cli)
endif()

if (RUN_TESTS)
add_executable(qamrpp_tests tests/qamrpp_tests.cpp)
set_target_properties(qamrpp_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(qamrpp_tests PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party/Catch2/src)
add_subdirectory(third_party/Catch2)
target_link_libraries(qamrpp_tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME qamrpp_tests COMMAND qamrpp_tests)
endif()
