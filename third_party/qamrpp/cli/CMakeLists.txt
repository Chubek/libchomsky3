# command-line utilities
option(INSTALL_CLI_TO_SYSTEM_BIN "Install CLI tools into CMAKE_INSTALL_BINDIR" OFF)

# CLI periphery files (manifest, syntax, frag) always live in $QAMRPP_HOME/cli;
# the tools resolve them from that exact path at runtime.
set(CLI_PERIPHERY_DIR "${QAMRPP_HOME}/cli")

if(INSTALL_CLI_TO_SYSTEM_BIN)
  set(_QAMRPP_CLI_BINDIR "${CMAKE_INSTALL_BINDIR}")
else()
  set(_QAMRPP_CLI_BINDIR "${QAMRPP_HOME}/bin")
endif()

set(PERIPHERY_CLI_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/QaMRpp-CLI.yaml"
  "${CMAKE_CURRENT_SOURCE_DIR}/lua.syntax"
  "${CMAKE_CURRENT_SOURCE_DIR}/lua.frag")

add_executable(qamrpp-cli qamrpp-cli.cpp)
set_target_properties(qamrpp-cli PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(qamrpp-cli PRIVATE
  QAMRPP_CLI_PERIPHERY_DIR="${CLI_PERIPHERY_DIR}")
if(NOT TARGET yaml-cpp)
  set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
  add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Klyspec/third_party/yaml-cpp
                   ${CMAKE_BINARY_DIR}/third_party/yaml-cpp)
endif()
target_include_directories(qamrpp-cli PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/Klyspec/include
  ${CMAKE_SOURCE_DIR}/third_party/Klyspec/third_party
  ${CMAKE_SOURCE_DIR}/third_party/Klyspec/third_party/yaml-cpp/include
  ${CMAKE_SOURCE_DIR}/third_party/PikoRL/include
  ${CMAKE_SOURCE_DIR}/third_party/SerdeTk/include)
target_link_libraries(qamrpp-cli PRIVATE yaml-cpp)

add_executable(qamrpp-assembler qamrpp-assembler.cpp)
set_target_properties(qamrpp-assembler PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(qamrpp-linker qamrpp-linker.cpp)
set_target_properties(qamrpp-linker PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(podpack podpack.cpp)
set_target_properties(podpack PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(podpack PRIVATE
  QAMRPP_CLI_PERIPHERY_DIR="${CLI_PERIPHERY_DIR}")
target_include_directories(podpack PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/SerdeTk/include)

install(FILES ${PERIPHERY_CLI_FILES} DESTINATION ${CLI_PERIPHERY_DIR})
install(TARGETS qamrpp-cli RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS qamrpp-assembler RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS qamrpp-linker RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS podpack RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
