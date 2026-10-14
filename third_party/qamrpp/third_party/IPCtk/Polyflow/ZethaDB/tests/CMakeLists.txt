function(add_noexec_wrapped_test test_name target_name)
  add_test(
    NAME ${test_name}
    COMMAND bash -c "set -euo pipefail; BIN=\"$1\"; RUN=\"/tmp/${test_name}\"; cp \"$BIN\" \"$RUN\"; chmod +x \"$RUN\"; \"$RUN\""
    _
    "$<TARGET_FILE:${target_name}>"
  )
  set_tests_properties(${test_name} PROPERTIES LABELS "noexec;full_pipeline")
endfunction()

add_executable(zethaquery_unit_tests unit/test_zethaquery_parser.cpp)
target_include_directories(zethaquery_unit_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
set_target_properties(zethaquery_unit_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_noexec_wrapped_test(zethaquery_unit_tests zethaquery_unit_tests)
set_tests_properties(zethaquery_unit_tests PROPERTIES LABELS "unit;noexec;full_pipeline")

add_executable(zethastorage_crud_unit_tests unit/test_zethastorage_crud.cpp)
target_include_directories(zethastorage_crud_unit_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
set_target_properties(zethastorage_crud_unit_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_noexec_wrapped_test(zethastorage_crud_unit_tests zethastorage_crud_unit_tests)
set_tests_properties(zethastorage_crud_unit_tests PROPERTIES LABELS "unit;noexec;full_pipeline")

add_executable(zethamem_stage8_unit_tests unit/test_zethamem_stage8.cpp)
target_include_directories(zethamem_stage8_unit_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
set_target_properties(zethamem_stage8_unit_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_noexec_wrapped_test(zethamem_stage8_unit_tests zethamem_stage8_unit_tests)
set_tests_properties(zethamem_stage8_unit_tests PROPERTIES LABELS "unit;noexec;full_pipeline")

add_executable(zetahrdb_stage9_unit_tests unit/test_zetahrdb_stage9.cpp)
target_include_directories(zetahrdb_stage9_unit_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
set_target_properties(zetahrdb_stage9_unit_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_noexec_wrapped_test(zetahrdb_stage9_unit_tests zetahrdb_stage9_unit_tests)
set_tests_properties(zetahrdb_stage9_unit_tests PROPERTIES LABELS "unit;noexec;full_pipeline")

add_executable(stage10_e2e_azmatest unit/test_stage10_e2e_wrapper.cpp)
target_include_directories(stage10_e2e_azmatest PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_definitions(stage10_e2e_azmatest PRIVATE ZETHADB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(stage10_e2e_azmatest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME stage10_e2e COMMAND bash ${CMAKE_SOURCE_DIR}/tests/e2e/smoke_stage10.sh)
set_tests_properties(stage10_e2e PROPERTIES LABELS "e2e;noexec;full_pipeline")
