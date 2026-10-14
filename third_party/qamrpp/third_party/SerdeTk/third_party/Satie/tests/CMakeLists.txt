# Unit-test suite — self-contained Catch2-compatible runner.
# tests/catch_shim.hpp provides the macro surface so no external Catch2
# dependency or network fetch is required.

add_executable(satie_tests
  satie_tests.cpp
  catch_shim.hpp
  test_common.hpp
  test_dsltk.hpp
  test_solvers.hpp)

target_link_libraries(satie_tests PRIVATE satie::satie)
target_include_directories(satie_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Compile flags inherited from the satie target; keep tests strict too.
target_compile_options(satie_tests PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic>)

include(CTest)
add_test(NAME satie_tests COMMAND satie_tests)
