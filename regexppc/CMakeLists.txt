add_library(chomsky3_lexer STATIC
    ../src/compiler/lexer.cpp
    ../src/compiler/simd.c
    ../src/regex/ere.c)
target_include_directories(chomsky3_lexer PUBLIC ../include)
target_compile_features(chomsky3_lexer PUBLIC cxx_std_17)

add_executable(regexppc regexppc.cpp preprocess.cpp re2c_style.cpp flex_style.cpp emitter.cpp lua.cpp)
target_compile_features(regexppc PRIVATE cxx_std_17)
target_link_libraries(regexppc PRIVATE chomsky3_lexer ${CMAKE_DL_LIBS})
target_include_directories(regexppc SYSTEM PRIVATE
    ../third_party/qamrpp/include
    ../third_party/qamrpp/third_party
    ../third_party/qamrpp/third_party/SerdeTk/include)
# Vendored interpreter headers have their own warning policy.
set_source_files_properties(lua.cpp PROPERTIES COMPILE_OPTIONS "-Wno-error")
if(REGEXPPC_BUILD_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME regexppc_end_to_end
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.py
            $<TARGET_FILE:regexppc> ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER}
            ${CMAKE_SOURCE_DIR})
endif()
if(CHOMSKY3_INSTALL)
    install(TARGETS regexppc RUNTIME DESTINATION bin)
    install(FILES runtime.hpp DESTINATION include/regexppc)
    install(DIRECTORY ../third_party/xsimd/include/xsimd DESTINATION include)
endif()
