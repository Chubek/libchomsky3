# command-line utilities
option(INSTALL_CLI_TO_SYSTEM_BIN "Install CLI tools into CMAKE_INSTALL_BINDIR" OFF)

if(INSTALL_CLI_TO_SYSTEM_BIN)
  set(_QAMRPP_CLI_BINDIR "${CMAKE_INSTALL_BINDIR}")
else()
  set(_QAMRPP_CLI_BINDIR "${QAMRPP_HOME}/bin")
endif()

add_executable(qamrpp-cli qamrpp-cli.cpp)
set_target_properties(qamrpp-cli PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(qamrpp-cli PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party/Klyspec)

add_executable(qamrpp-assembler qamrpp-assembler.cpp)
set_target_properties(qamrpp-assembler PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(qamrpp-linker qamrpp-linker.cpp)
set_target_properties(qamrpp-linker PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(podpack podpack.cpp)
set_target_properties(podpack PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(podpack PRIVATE ${CMAKE_SOURCE_DIR})

install(TARGETS qamrpp-cli RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS qamrpp-assembler RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS qamrpp-linker RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
install(TARGETS podpack RUNTIME DESTINATION ${_QAMRPP_CLI_BINDIR})
