cmake_minimum_required(VERSION 3.20)
project(SerdeTk LANGUAGES C CXX)

include(GNUInstallDirs)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/lemon)
configure_file(cmake/lemon-config.h.in ${CMAKE_BINARY_DIR}/generated/lemon/config.h @ONLY)

add_library(DSLtk INTERFACE)
target_include_directories(DSLtk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/PikoRL/third_party)

add_library(SerdeTk INTERFACE)
target_include_directories(SerdeTk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(SerdeTk INTERFACE DSLtk)

add_library(MiniZIP INTERFACE)
target_include_directories(MiniZIP INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(MiniZIP INTERFACE DSLtk)

add_library(SerdeTkSatie STATIC third_party/Satie/src/Satie.cpp)
target_include_directories(SerdeTkSatie PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Satie/include)
target_link_libraries(SerdeTkSatie PUBLIC DSLtk)
target_compile_features(SerdeTkSatie PUBLIC cxx_std_20)

add_executable(serdetk-cli cli/SerdeTk-CLI.cpp)
target_include_directories(serdetk-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/PikoRL/third_party)
target_link_libraries(serdetk-cli PRIVATE SerdeTk SerdeTkSchemaRegistry MiniZIPPackageIntegrity)
target_compile_features(serdetk-cli PRIVATE cxx_std_20)

add_subdirectory(stdspec)
add_subdirectory(stdplugin/SerdeTk/Semver)
add_subdirectory(stdplugin/SerdeTk/SchemaRegistry)
add_subdirectory(stdplugin/SerdeTk/XPath)
add_subdirectory(stdplugin/MiniZIP/PackageIntegrity)
add_subdirectory(tests/SerdeTk)

install(TARGETS serdetk-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/SerdeTk.hpp include/SerdeTk-Plugin.hpp include/SerdeTk-Frontend.hpp
    include/MiniZIP.hpp include/MiniZIP-Plugin.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
