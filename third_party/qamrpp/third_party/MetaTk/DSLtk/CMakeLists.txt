project(DSLtk VERSION 1.0 LANGUAGES CXX)

add_library(DSLtk INTERFACE)

target_include_directories(DSLtk
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(DSLtk INTERFACE cxx_std_20)

install(
    FILES DSLtk.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS DSLtk
    EXPORT DSLtkTargets
)

install(
    EXPORT DSLtkTargets
    FILE DSLtkTargets.cmake
    NAMESPACE DSLtk::
    DESTINATION lib/cmake/DSLtk
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/DSLtkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DSLtk
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake"
    DESTINATION lib/cmake/DSLtk
)

