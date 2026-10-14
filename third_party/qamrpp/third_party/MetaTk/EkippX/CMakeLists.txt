project(EkippX VERSION 1.0 LANGUAGES CXX)

add_library(EkippX
    KlyspecBridge.cpp
    PikoRLBridge.cpp
)

target_include_directories(EkippX
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EkippX
    PUBLIC
        DSLtk
        Parzek
)

if(TARGET Klyspec)
    target_link_libraries(EkippX PUBLIC Klyspec)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/Klyspec/Klyspec.hpp")
    target_include_directories(EkippX PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/Klyspec>)
endif()

if(TARGET PikoRL)
    target_link_libraries(EkippX PUBLIC PikoRL)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/PikoRL/PikoRL.hpp")
    target_include_directories(EkippX PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/PikoRL>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/PikoRL/third_party>)
endif()

if(TARGET SerdeTk)
    target_link_libraries(EkippX PUBLIC SerdeTk)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/SerdeTk/include/SerdeTk.hpp")
    target_include_directories(EkippX PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/SerdeTk/include>)
endif()

if(TARGET QaMRpp)
    target_link_libraries(EkippX PUBLIC QaMRpp)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/QaMRpp/include/QaMRpp.hpp")
    target_include_directories(EkippX PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/QaMRpp/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party>)
endif()

target_compile_features(EkippX PUBLIC cxx_std_20)

add_executable(ekippx
    EkippX-CLI.cpp
)

target_link_libraries(ekippx PRIVATE EkippX)

install(
    TARGETS
        EkippX
        ekippx
    EXPORT EkippXTargets
)

install(
    FILES
        EkippX.hpp
        EkippX-Batteries.hpp
        EkippX-Plugin.hpp
        EkippX-PluginAPI.hpp
    DESTINATION include
)

install(
    DIRECTORY manifests
    DESTINATION share/EkippX
)

install(
    EXPORT EkippXTargets
    FILE EkippXTargets.cmake
    NAMESPACE EkippX::
    DESTINATION lib/cmake/EkippX
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/EkippXConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/EkippXConfig.cmake"
    INSTALL_DESTINATION lib/cmake/EkippX
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/EkippXConfig.cmake"
    DESTINATION lib/cmake/EkippX
)

if(BUILD_TESTING)

    enable_testing()

    file(GLOB TESTS tests/*.cpp)

    foreach(src ${TESTS})

        get_filename_component(name ${src} NAME_WE)

        add_executable(${name} ${src})

        target_link_libraries(${name}
            PRIVATE
                EkippX
                Catch2::Catch2WithMain
        )

        add_test(NAME ${name} COMMAND ${name})

    endforeach()

endif()

if(BUILD_EXAMPLES)

    file(GLOB EXAMPLES examples/*.cpp)

    foreach(src ${EXAMPLES})

        get_filename_component(name ${src} NAME_WE)

        add_executable(${name} ${src})

        target_link_libraries(${name}
            PRIVATE
                EkippX
        )

    endforeach()

endif()