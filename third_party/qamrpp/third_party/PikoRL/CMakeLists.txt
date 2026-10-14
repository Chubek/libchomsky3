cmake_minimum_required(VERSION 3.16)

project(PikoRL VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)

if(DEFINED ENV{PIKORL_HOME} AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "$ENV{PIKORL_HOME}" CACHE PATH
      "PikoRL runtime installation root" FORCE)
endif()

option(PICORL_BUILD_DOCS "Build Doxygen documentation" ON)

add_subdirectory(qamrpp-lpikorl)

add_executable(pikorl-cli
  pikorl-cli.cpp
)
target_compile_features(pikorl-cli PRIVATE cxx_std_17)
set_target_properties(pikorl-cli PROPERTIES OUTPUT_NAME pikorl)
install(TARGETS pikorl-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES man/man1/pikrl-cli.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(DIRECTORY DESTINATION addons)
install(DIRECTORY DESTINATION plugins)

add_subdirectory(stdext)
add_subdirectory(extensions)
add_subdirectory(interop)
add_subdirectory(examples)

if(PICORL_BUILD_DOCS)
  add_subdirectory(docs)
endif()
