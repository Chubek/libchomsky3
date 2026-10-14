cmake_minimum_required(VERSION 3.16)
project(QaMRppStdLib C CXX)

option(BUILD_CLI "Build qamrpp CLI tools" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED ENV{HOME})
  message(FATAL_ERROR "HOME is not set")
endif()

set(QAMRPP_INSTALL_DIR "$ENV{HOME}/.qamrpp")

add_library(qamrpp_library OBJECT QaMRpp-Library.c)

function(add_qamrpp_stdlib name)
  add_library(qamrpp_${name} SHARED stdlib/${name}.c $<TARGET_OBJECTS:qamrpp_library>)
  set_target_properties(qamrpp_${name} PROPERTIES PREFIX "libqamrpp_" OUTPUT_NAME "${name}")
  install(TARGETS qamrpp_${name} LIBRARY DESTINATION ${QAMRPP_INSTALL_DIR})
endfunction()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/qamrpp.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/qamrpp.pc
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/qamrpp.pc
    DESTINATION lib/pkgconfig
)

add_qamrpp_stdlib(core)
add_qamrpp_stdlib(string)
add_qamrpp_stdlib(table)
add_qamrpp_stdlib(math)
add_qamrpp_stdlib(io)
add_qamrpp_stdlib(os)
add_qamrpp_stdlib(debug)
add_qamrpp_stdlib(coroutine)
add_qamrpp_stdlib(package)
add_qamrpp_stdlib(utf8)

if(BUILD_CLI)
  add_subdirectory(cli)
endif()
