add_executable(ipctk_unit_tests
  unit/test_main.cpp
  unit/test_1.cpp
  unit/test_2.cpp
  unit/test_3.cpp
  unit/test_4.cpp
  unit/test_5.cpp
  unit/test_6.cpp
  unit/test_7.cpp
  unit/test_8.cpp
  unit/test_9.cpp
  unit/test_10.cpp
  unit/test_11.cpp
  unit/test_12.cpp
  unit/test_13.cpp
  unit/test_14.cpp
  unit/test_15.cpp
  unit/test_16.cpp
  unit/test_17.cpp
  unit/test_18.cpp
  unit/test_19.cpp
  unit/test_20.cpp
  unit/test_21.cpp
  unit/test_22.cpp
  unit/test_23.cpp
  unit/test_24.cpp
  unit/test_25.cpp
)
target_link_libraries(ipctk_unit_tests PRIVATE ipctk)
find_package(Catch2 3 QUIET)
if(Catch2_FOUND)
  target_link_libraries(ipctk_unit_tests PRIVATE Catch2::Catch2WithMain)
  target_compile_definitions(ipctk_unit_tests PRIVATE IPCTK_HAS_CATCH2_WITH_MAIN=1)
endif()
add_test(NAME ipctk.unit COMMAND ipctk_unit_tests)

add_executable(ipctk_fuzz_target fuzz/fuzz_target.cpp)
target_link_libraries(ipctk_fuzz_target PRIVATE ipctk)
