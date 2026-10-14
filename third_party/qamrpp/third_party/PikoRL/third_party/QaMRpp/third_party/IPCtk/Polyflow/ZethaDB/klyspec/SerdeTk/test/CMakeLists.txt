add_executable(parser_workflow_tests unit/test_parser_workflow.cpp ${CMAKE_SOURCE_DIR}/AzmaTest/AzmaIDL.c)
target_include_directories(parser_workflow_tests PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/AzmaTest)
target_compile_features(parser_workflow_tests PRIVATE cxx_std_20)

add_executable(azmaidl_fuzz_parser fuzz/fuzz_azmaidl_parser.c ${CMAKE_SOURCE_DIR}/AzmaTest/AzmaIDL.c)
target_include_directories(azmaidl_fuzz_parser PRIVATE ${CMAKE_SOURCE_DIR}/AzmaTest)
