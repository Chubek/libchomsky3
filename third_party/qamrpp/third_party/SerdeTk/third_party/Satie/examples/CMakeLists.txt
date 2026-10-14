# Examples — compiled automatically by the root CMakeLists.txt.
# Each C++ example links the engine library; DSL text files are data only.

add_executable(example_basic_solve  API/basic_solve.cpp)
target_link_libraries(example_basic_solve  PRIVATE satie::satie)

add_executable(example_build_ast   NativeDSL/build_ast.cpp)
target_link_libraries(example_build_ast   PRIVATE satie::satie)

# DSL text examples (*.satie) are not compiled; install them as data so
# users can load them via the REPL/CLI.
install(FILES DSL/nqueens.satie DSL/pigeonhole.satie
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/examples)
