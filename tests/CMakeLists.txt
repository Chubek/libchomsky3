find_package(Catch2 3 REQUIRED)

enable_language(CXX)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 v3 not found - tests will be skipped")
    return()
endif()

message(STATUS "Catch2 v3 found - building tests")

list(APPEND CMAKE_MODULE_PATH
    "${Catch2_SOURCE_DIR}/extras"
    "${Catch2_DIR}"
)
include(Catch)

file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")
file(GLOB_RECURSE FUZZ_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp")

if(UNIT_TEST_SOURCES)
    foreach(test_source IN LISTS UNIT_TEST_SOURCES)
        get_filename_component(test_name "${test_source}" NAME_WE)
        set(test_target "chomsky3_${test_name}")

        add_executable(${test_target} "${test_source}")
        target_link_libraries(${test_target}
            PRIVATE
                chomsky3
                Catch2::Catch2WithMain
        )
        target_include_directories(${test_target}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/include
        )
        set_target_properties(${test_target} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )

        catch_discover_tests(${test_target})
    endforeach()
endif()

if(FUZZ_TEST_SOURCES)
    foreach(test_source IN LISTS FUZZ_TEST_SOURCES)
        get_filename_component(test_name "${test_source}" NAME_WE)
        set(test_target "chomsky3_${test_name}")

        add_executable(${test_target} "${test_source}")
        target_link_libraries(${test_target} PRIVATE chomsky3)
        target_include_directories(${test_target}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/include
        )
        set_target_properties(${test_target} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )

        if(DEFINED ENV{AFL_CC})
            set_target_properties(${test_target} PROPERTIES
                COMPILE_FLAGS "-O1"
            )
            add_custom_target("fuzz_${test_target}"
                COMMAND "${test_target}"
                DEPENDS "${test_target}"
                COMMENT "Build AFL fuzz target ${test_target} (manual harness execution)"
            )
        endif()
    endforeach()
endif()
