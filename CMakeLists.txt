cmake_minimum_required(VERSION 3.15)

project(libchomsky3
    VERSION 1.0.0
    DESCRIPTION "High-performance regex engine with JIT compilation"
    LANGUAGES C
)

# ============================================================================
# Project Options
# ============================================================================

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(CHOMSKY3_BUILD_TESTS "Build test suite" OFF)
option(CHOMSKY3_BUILD_EXAMPLES "Build example programs" OFF)
option(CHOMSKY3_ENABLE_JIT "Enable JIT compilation support via SLJIT" ON)
option(CHOMSKY3_INSTALL "Generate install target" ON)

# ============================================================================
# Compiler Settings
# ============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ============================================================================
# Main Library Target
# ============================================================================

if(CHOMSKY3_ENABLE_JIT)
    set(SLJIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sljit")

    if(EXISTS "${SLJIT_DIR}/sljit_src/sljitLir.h")
        message(STATUS "Found SLJIT in third_party/sljit")

        add_library(sljit STATIC
            ${SLJIT_DIR}/sljit_src/sljitLir.c
            ${SLJIT_DIR}/regex_src/regexJIT.c
            ${SLJIT_DIR}/regex_src/regexMain.c
        )

        target_include_directories(sljit PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${SLJIT_DIR}/sljit_src
            ${SLJIT_DIR}/regex_src
	    o
        )

        target_compile_definitions(sljit PRIVATE
            SLJIT_CONFIG_AUTO=1
            SLJIT_VERBOSE=0
        )

        set(CHOMSKY3_JIT_ENABLED TRUE)
    else()
        message(WARNING "SLJIT not found in third_party/sljit - JIT disabled")
        set(CHOMSKY3_JIT_ENABLED FALSE)
    endif()
else()
    set(CHOMSKY3_JIT_ENABLED FALSE)
endif()

file(GLOB_RECURSE CHOMSKY3_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(chomsky3 ${CHOMSKY3_SOURCES})

target_include_directories(chomsky3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(chomsky3 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER include/chomsky3.h
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(chomsky3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicode_names)

if(CHOMSKY3_ENABLE_JIT)
    target_compile_definitions(chomsky3 PRIVATE CHOMSKY3_BUILD)
    target_include_directories(chomsky3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

target_link_libraries(chomsky3 PRIVATE m)

# ============================================================================
# SLJIT Integration (JIT Compilation Support)
# ============================================================================

if(TARGET sljit)
    target_link_libraries(chomsky3 PRIVATE sljit)
    target_include_directories(chomsky3 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${SLJIT_DIR}
        ${SLJIT_DIR}/sljit_src
        ${SLJIT_DIR}/regex_src
    )
    target_compile_definitions(chomsky3 PUBLIC CHOMSKY3_HAS_JIT=1)
else()
    message(STATUS "CHOMSKY3_HAS_JIT disabled: SLJIT target unavailable")
endif()

# ============================================================================
# Subdirectories
# ============================================================================

if(CHOMSKY3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CHOMSKY3_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

add_subdirectory(docs)

# ============================================================================
# Installation
# ============================================================================

if(CHOMSKY3_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS chomsky3
        EXPORT chomsky3Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT chomsky3Targets
        FILE chomsky3Targets.cmake
        NAMESPACE chomsky3::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/chomsky3Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/chomsky3
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/chomsky3.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/chomsky3.pc
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chomsky3.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()

# ============================================================================
# Build Summary
# ============================================================================

message(STATUS "")
message(STATUS "Chomsky3 Configuration Summary")
message(STATUS "==============================")
message(STATUS "Version:           ${PROJECT_VERSION}")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared libraries:  ${BUILD_SHARED_LIBS}")
message(STATUS "JIT support:       ${CHOMSKY3_JIT_ENABLED}")
message(STATUS "Build tests:       ${CHOMSKY3_BUILD_TESTS}")
message(STATUS "Build examples:    ${CHOMSKY3_BUILD_EXAMPLES}")
message(STATUS "Install target:    ${CHOMSKY3_INSTALL}")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
