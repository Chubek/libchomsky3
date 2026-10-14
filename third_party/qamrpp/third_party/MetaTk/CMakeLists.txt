cmake_minimum_required(VERSION 3.24)

project(
    MetaTk
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(BUILD_TESTING "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

# ------------------------------------------------------------
# Third party
# ------------------------------------------------------------

if(BUILD_TESTING)
    set(_catch2_dir "${CMAKE_SOURCE_DIR}/third_party/Catch2")
    if(EXISTS "${_catch2_dir}/CMakeLists.txt")
        add_subdirectory(third_party/Catch2 EXCLUDE_FROM_ALL)
    elseif(EXISTS "${_catch2_dir}/src/catch2")
        file(GLOB_RECURSE _catch2_sources CONFIGURE_DEPENDS
            "${_catch2_dir}/src/catch2/*.cpp")

        add_library(catch2_with_main STATIC ${_catch2_sources})
        target_include_directories(
            catch2_with_main
            PUBLIC
                $<BUILD_INTERFACE:${_catch2_dir}/src>
        )
        add_library(Catch2::Catch2WithMain ALIAS catch2_with_main)
    else()
        message(WARNING
            "Catch2 is unavailable: neither third_party/Catch2/CMakeLists.txt nor "
            "third_party/Catch2/src/catch2 exists. BUILD_TESTING requires "
            "Catch2 and tests may fail at build time.")
    endif()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/SerdeTk/CMakeLists.txt")
    add_subdirectory(third_party/SerdeTk EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/QaMRpp/CMakeLists.txt")
    add_subdirectory(third_party/QaMRpp EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/Klyspec/CMakeLists.txt")
    add_subdirectory(third_party/Klyspec EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/PikoRL/CMakeLists.txt")
    add_subdirectory(third_party/PikoRL EXCLUDE_FROM_ALL)
endif()

# ------------------------------------------------------------
# Libraries
# ------------------------------------------------------------

add_subdirectory(DSLtk)
add_subdirectory(Parzek)
add_subdirectory(EkippX)
