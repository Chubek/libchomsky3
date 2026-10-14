cmake_minimum_required(VERSION 3.16)
project(Jiterati VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(JITERATI_BUILD_TESTS "Build Jiterati tests" OFF)
option(JITERATI_BUILD_EXAMPLES "Build Jiterati examples" OFF)
option(JITERATI_BUILD_LUA "Build Lua/Sol2 bindings" ON)
option(JITERATI_BUILD_DOCS "Enable the documentation target" ON)

# ---------------------------------------------------------------------------
# pkg-config: configure jiterati.pc
# ---------------------------------------------------------------------------
set(JITERATI_PC_FILE ${CMAKE_CURRENT_BINARY_DIR}/jiterati.pc)
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/jiterati.pc.in"
    "${JITERATI_PC_FILE}"
    @ONLY
)

# ---------------------------------------------------------------------------
# Public headers
# ---------------------------------------------------------------------------
set(JITERATI_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Jiterati.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Jiterati-BE.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Jiterati-Macro.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Jiterati-Pass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Jiterati-Plugin.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSLtk.hpp
)

# Backend sources
set(JITERATI_BACKEND_SOURCES
    src/JIT.cpp
    BE/Metadata.cpp
    BE/AMD64/AMD64.cpp
    BE/AMD64/Backend.cpp
    BE/AMD64/ISel.cpp
    BE/AMD64/RegAlloc.cpp
    BE/AMD64/Rewrite.cpp
    BE/AMD64/Peephole.cpp
    BE/AMD64/Emit.cpp
    BE/AMD64/IRCompiler.cpp
    BE/AArch64/AArch64.cpp
    BE/AArch64/ISel.cpp
    BE/AArch64/RegAlloc.cpp
    BE/AArch64/Rewrite.cpp
    BE/AArch64/Peephole.cpp
    BE/AArch64/Emit.cpp
    BE/AArch64/IRCompiler.cpp
    BE/RV64/RV64.cpp
    BE/RV64/ISel.cpp
    BE/RV64/RegAlloc.cpp
    BE/RV64/Rewrite.cpp
    BE/RV64/Peephole.cpp
    BE/RV64/Emit.cpp
    BE/RV64/IRCompiler.cpp
    BE/WASM/WASM.cpp
    BE/WASM/ISel.cpp
    BE/WASM/RegAlloc.cpp
    BE/WASM/Rewrite.cpp
    BE/WASM/Peephole.cpp
    BE/WASM/Emit.cpp
    BE/WASM/IRCompiler.cpp
    IR/AST.cpp
    IR/TAC.cpp
    IR/Terse.cpp
    IR/Analyze.cpp
    IR/Rewrite.cpp
    IR/Peephole.cpp
    IR/Parse.cpp
)

# Pass library sources
set(JITERATI_PASS_SOURCES
    Passlib/CFGAnalysis.cpp
    Passlib/DominatorAnalysis.cpp
    Passlib/LivenessAnalysis.cpp
    Passlib/ConstantFolding.cpp
    Passlib/ConstantPropagation.cpp
    Passlib/DeadCodeRemoval.cpp
    Passlib/StrengthReduction.cpp
    Passlib/PeepholeOptimization.cpp
)

# Plugin sources
set(JITERATI_PLUGIN_SOURCES
    Plugins/BURSISel.cpp
    Plugins/MaxMunchISel.cpp
    Plugins/ListISched.cpp
)

# DSL / JBL / JPL
set(JITERATI_DSL_SOURCES
    src/Jiterati-JBL.cpp
    src/Jiterati-JPL.cpp
)

# Lua bindings
set(JITERATI_LUA_SOURCES
    Lua/JiteratiBinding.cpp
)


if(JITERATI_BUILD_DOCS)
    add_subdirectory(docs)

    # Build docs during installation
    install(CODE "
        message(STATUS \"Building Jiterati documentation...\")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target docs
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    ")

    # Install generated HTML documentation
    install(
        DIRECTORY "${CMAKE_BINARY_DIR}/docs/html"
        DESTINATION share/doc/jiterati
    )
endif()


# ---------------------------------------------------------------------------
# Core library: backends + passes + plugins + DSLs
# ---------------------------------------------------------------------------
add_library(jiterati STATIC
    ${JITERATI_BACKEND_SOURCES}
    ${JITERATI_PASS_SOURCES}
    ${JITERATI_PLUGIN_SOURCES}
    ${JITERATI_DSL_SOURCES}
)

target_include_directories(jiterati PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# ---------------------------------------------------------------------------
# Install vendored third_party components
# ---------------------------------------------------------------------------

# Base install location for third-party dependencies
set(JITERATI_TP_INSTALL_DIR include/jiterati/third_party)

# CLI11 (header-only)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/CLI11/
    DESTINATION ${JITERATI_TP_INSTALL_DIR}/CLI11
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# sol2 (header-only)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/sol2/include/
    DESTINATION ${JITERATI_TP_INSTALL_DIR}/sol2/include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Lua (vendored headers — if you ship your own Lua)
# If you rely on system Lua, remove this section.
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/lua/
    DESTINATION ${JITERATI_TP_INSTALL_DIR}/lua
    FILES_MATCHING PATTERN "*.h"
)

# zstd (headers)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/zstd/
    DESTINATION ${JITERATI_TP_INSTALL_DIR}/zstd
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# zstd library (static)
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/zstd/libzstd.a)
    install(
        FILES ${CMAKE_SOURCE_DIR}/third_party/zstd/libzstd.a
        DESTINATION lib
    )
endif()


# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(JITERATI_BUILD_EXAMPLES)
    add_executable(jiterati_square examples/square.cpp)
    target_link_libraries(jiterati_square PRIVATE jiterati)

    add_executable(jiterati_abs examples/abs.cpp)
    target_link_libraries(jiterati_abs PRIVATE jiterati)

    file(GLOB JITERATI_API_EXAMPLES CONFIGURE_DEPENDS examples/API/*.cpp)
    foreach(example_source IN LISTS JITERATI_API_EXAMPLES)
        get_filename_component(example_name ${example_source} NAME_WE)
        set(example_target jiterati_api_${example_name})
        add_executable(${example_target} ${example_source})
        target_link_libraries(${example_target} PRIVATE jiterati)
    endforeach()
endif()

add_executable(Jiterati-CLI src/Jiterati-CLI.cpp)
target_link_libraries(Jiterati-CLI PRIVATE jiterati)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(JITERATI_BUILD_TESTS)
    enable_testing()

    add_executable(jiterati_test_core tests/test_core.cpp)
    target_link_libraries(jiterati_test_core PRIVATE jiterati)
    add_test(NAME core COMMAND jiterati_test_core)

    add_executable(jiterati_test_passes tests/test_passes.cpp)
    target_link_libraries(jiterati_test_passes PRIVATE jiterati)
    add_test(NAME passes COMMAND jiterati_test_passes)

    add_executable(jiterati_test_jbl tests/test_jbl.cpp)
    target_link_libraries(jiterati_test_jbl PRIVATE jiterati)
    add_test(NAME jbl COMMAND jiterati_test_jbl)

    add_executable(jiterati_test_plugins tests/test_plugins.cpp)
    target_link_libraries(jiterati_test_plugins PRIVATE jiterati)
    add_test(NAME plugins COMMAND jiterati_test_plugins)

    add_executable(jiterati_test_backend_metadata tests/test_backend_metadata.cpp)
    target_link_libraries(jiterati_test_backend_metadata PRIVATE jiterati)
    add_test(NAME backend_metadata COMMAND jiterati_test_backend_metadata)

    if(JITERATI_BUILD_LUA)
        add_executable(jiterati_test_lua tests/test_lua.cpp)
        target_link_libraries(jiterati_test_lua PRIVATE jiterati)
        add_test(NAME lua COMMAND jiterati_test_lua)
    endif()

    add_test(NAME cli_help COMMAND Jiterati-CLI help)
    add_test(NAME cli_parse COMMAND Jiterati-CLI parse ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid.terse)
    add_test(NAME cli_validate COMMAND Jiterati-CLI validate ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid.terse)
    add_test(NAME cli_format COMMAND Jiterati-CLI format ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid.terse)
    add_test(NAME cli_compile COMMAND Jiterati-CLI compile --target amd64 --emit asm ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid.terse)
    add_test(NAME cli_package_pack COMMAND Jiterati-CLI package --pack ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/package/package.sh --out ${CMAKE_CURRENT_BINARY_DIR}/cli-fixture.jpkg)
    add_test(NAME cli_package_validate COMMAND Jiterati-CLI package --validate ${CMAKE_CURRENT_BINARY_DIR}/cli-fixture.jpkg)
    add_test(NAME cli_package_manifest COMMAND Jiterati-CLI package --view-manifest ${CMAKE_CURRENT_BINARY_DIR}/cli-fixture.jpkg)
    add_test(NAME cli_package_list_plugins COMMAND Jiterati-CLI package --list-plugins ${CMAKE_CURRENT_BINARY_DIR}/cli-fixture.jpkg)
    add_test(NAME cli_package_malformed COMMAND Jiterati-CLI package --validate ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/malformed/missing-name.jpkg)
    add_test(NAME cli_package_pack_rejects_undeclared_file COMMAND Jiterati-CLI package --pack ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/package-extra/package.sh --out ${CMAKE_CURRENT_BINARY_DIR}/bad-extra.jpkg)
    add_test(NAME cli_package_install COMMAND ${CMAKE_COMMAND} -E env JITERATI_HOME=${CMAKE_CURRENT_BINARY_DIR}/jiterati-home $<TARGET_FILE:Jiterati-CLI> package --install ${CMAKE_CURRENT_BINARY_DIR}/cli-fixture.jpkg)
    add_test(NAME cli_package_verify COMMAND ${CMAKE_COMMAND} -E env JITERATI_HOME=${CMAKE_CURRENT_BINARY_DIR}/jiterati-home $<TARGET_FILE:Jiterati-CLI> package --verify cli-fixture)
    add_test(NAME cli_package_remove COMMAND ${CMAKE_COMMAND} -E env JITERATI_HOME=${CMAKE_CURRENT_BINARY_DIR}/jiterati-home $<TARGET_FILE:Jiterati-CLI> package --remove cli-fixture)
    set_tests_properties(cli_package_validate cli_package_manifest cli_package_list_plugins PROPERTIES DEPENDS cli_package_pack)
    set_tests_properties(cli_package_malformed cli_package_pack_rejects_undeclared_file PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(cli_package_install PROPERTIES DEPENDS cli_package_pack)
    set_tests_properties(cli_package_verify PROPERTIES DEPENDS cli_package_install)
    set_tests_properties(cli_package_remove PROPERTIES DEPENDS cli_package_verify)

    file(GLOB JITERATI_IR_EXAMPLES CONFIGURE_DEPENDS examples/IR/*.ir)
    foreach(example_ir IN LISTS JITERATI_IR_EXAMPLES)
        get_filename_component(example_name ${example_ir} NAME_WE)
        add_test(NAME ir_example_${example_name} COMMAND Jiterati-CLI validate ${example_ir})
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(FILES ${JITERATI_PUBLIC_HEADERS} DESTINATION include/jiterati)
install(TARGETS jiterati ARCHIVE DESTINATION lib)
install(TARGETS Jiterati-CLI RUNTIME DESTINATION bin)

# Install pkg-config file
install(
    FILES "${JITERATI_PC_FILE}"
    DESTINATION lib/pkgconfig
)
