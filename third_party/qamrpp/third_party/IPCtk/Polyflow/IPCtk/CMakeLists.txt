cmake_minimum_required(VERSION 3.16)
project(IPCtk VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(IPCTK_BUILD_EXAMPLES "Build IPCtk examples" ON)
option(IPCTK_BUILD_TESTS "Build IPCtk tests" ON)
option(IPCTK_BUILD_DOCS "Enable Doxygen docs target" ON)
option(IPCTK_ENABLE_BINDINGS "Enable SWIG bindings generation targets" ON)

add_library(ipctk INTERFACE)
target_include_directories(ipctk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(ipctk INTERFACE cxx_std_20)

install(FILES IPCtk.hpp DSLUtils.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS ipctk EXPORT IPCtkTargets)

if(IPCTK_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(IPCTK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

add_subdirectory(dest)

if(IPCTK_ENABLE_BINDINGS)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_custom_target(bindings-python
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bindings/GenerateBindings.py --lang python --out-dir ${CMAKE_CURRENT_BINARY_DIR}/bindings
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating Python SWIG bindings")
    add_custom_target(bindings-ruby
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bindings/GenerateBindings.py --lang ruby --out-dir ${CMAKE_CURRENT_BINARY_DIR}/bindings
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating Ruby SWIG bindings")
  endif()
endif()

if(IPCTK_BUILD_DOCS)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating Doxygen documentation")
  endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ipctk.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/ipctk.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ipctk.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(EXPORT IPCtkTargets
  FILE IPCtkTargets.cmake
  NAMESPACE IPCtk::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IPCtk)
