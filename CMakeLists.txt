cmake_minimum_required(VERSION 3.15)

project(libchomsky3
    VERSION 1.0.0
    DESCRIPTION "High-performance regex engine with JIT compilation"
    LANGUAGES C
)

# Project options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(CHOMSKY3_BUILD_TESTS "Build test suite" ON)
option(CHOMSKY3_BUILD_EXAMPLES "Build example programs" ON)
option(CHOMSKY3_ENABLE_JIT "Enable JIT compilation via SLJIT" ON)
option(CHOMSKY3_INSTALL "Generate install target" ON)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Include modules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find dependencies
if(CHOMSKY3_ENABLE_JIT)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(SLJIT sljit)
    endif()
    
    if(NOT SLJIT_FOUND)
        # Fallback: look for sljit as subdirectory or system install
        find_path(SLJIT_INCLUDE_DIR sljitLir.h
            PATHS /usr/include /usr/local/include
        )
        find_library(SLJIT_LIBRARY sljit
            PATHS /usr/lib /usr/local/lib
        )
        
        if(SLJIT_INCLUDE_DIR AND SLJIT_LIBRARY)
            set(SLJIT_FOUND TRUE)
            set(SLJIT_INCLUDE_DIRS ${SLJIT_INCLUDE_DIR})
            set(SLJIT_LIBRARIES ${SLJIT_LIBRARY})
        endif()
    endif()
    
    if(NOT SLJIT_FOUND)
        message(WARNING "SLJIT not found. JIT support will be disabled.")
        set(CHOMSKY3_ENABLE_JIT OFF)
    endif()
endif()

# Main library target
add_library(chomsky3
    src/chomsky3.c
    src/compiler.c
    src/vm.c
    src/optimizer.c
    src/jit.c
    src/unicode.c
    src/error.c
    src/uncide_names.c
)

# Library properties
set_target_properties(chomsky3 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER include/chomsky3.h
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Include directories
target_include_directories(chomsky3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE>:${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicode_names
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
target_compile_definitions(chomsky3 PRIVATE
    CHOMSKY3_BUILD
    $<$<CONFIG:Debug>:CHOMSKY3_DEBUG>
)

if(CHOMSKY3_ENABLE_JIT)
    target_compile_definitions(chomsky3 PUBLIC CHOMSKY3_JIT_ENABLED)
    target_include_directories(chomsky3 PRIVATE ${SLJIT_INCLUDE_DIRS})
    target_link_libraries(chomsky3 PRIVATE ${SLJIT_LIBRARIES})
endif()

# Link libraries
target_link_libraries(chomsky3 PRIVATE m)

# Tests
if(CHOMSKY3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(CHOMSKY3_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Documentation
add_subdirectory(docs)

# Installation
if(CHOMSKY3_INSTALL)
    # Install library and headers
    install(TARGETS chomsky3
        EXPORT chomsky3Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    
    # Install CMake config files
    install(EXPORT chomsky3Targets
        FILE chomsky3Targets.cmake
        NAMESPACE chomsky3::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )
    
    # Generate config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/chomsky3Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )
    
    # Generate version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    # Install config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )
    
    # Install pkg-config file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/chomsky3.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3.pc
        @ONLY
    )
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chomsky3.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "libchomsky3 ${PROJECT_VERSION} configuration:")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  JIT enabled:      ${CHOMSKY3_ENABLE_JIT}")
message(STATUS "  Build tests:      ${CHOMSKY3_BUILD_TESTS}")
message(STATUS "  Build examples:   ${CHOMSKY3_BUILD_EXAMPLES}")
message(STATUS "  Install target:   ${CHOMSKY3_INSTALL}")
message(STATUS "")
