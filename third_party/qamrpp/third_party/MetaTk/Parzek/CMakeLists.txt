project(Parzek VERSION 1.0 LANGUAGES CXX)

add_library(Parzek
    Parzek.cpp
)

target_include_directories(Parzek
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Parzek
    PUBLIC
        DSLtk
)

target_compile_features(Parzek PUBLIC cxx_std_20)

add_executable(parzek
    main.cpp
)

target_link_libraries(parzek PRIVATE Parzek)

install(
    TARGETS
        Parzek
        parzek
    EXPORT ParzekTargets
)

install(
    FILES Parzek.hpp
    DESTINATION include
)

install(
    EXPORT ParzekTargets
    FILE ParzekTargets.cmake
    NAMESPACE Parzek::
    DESTINATION lib/cmake/Parzek
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/ParzekConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ParzekConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Parzek
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ParzekConfig.cmake"
    DESTINATION lib/cmake/Parzek
)

if(BUILD_EXAMPLES)
    file(GLOB_RECURSE EXAMPLE_GRAMMARS examples/*)
endif()

if(BUILD_TESTING)

    enable_testing()

    file(GLOB TESTS tests/*.cpp tests/unit/*.cpp)

    foreach(src ${TESTS})

        get_filename_component(name ${src} NAME_WE)

        add_executable(${name} ${src})

        target_link_libraries(${name}
            PRIVATE
                Parzek
                Catch2::Catch2WithMain
        )

        add_test(NAME ${name} COMMAND ${name})

    endforeach()

endif()