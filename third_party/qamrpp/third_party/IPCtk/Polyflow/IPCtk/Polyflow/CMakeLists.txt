cmake_minimum_required(VERSION 3.20)
project(Polyflow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library interface
add_library(polyflow INTERFACE)
target_include_directories(polyflow INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Grafitt
    ${CMAKE_CURRENT_SOURCE_DIR}/ZethaDB
    ${CMAKE_CURRENT_SOURCE_DIR}/IPCtk
    ${CMAKE_CURRENT_SOURCE_DIR}/AzmaTest
)

# External monitor binary
add_executable(polyflow-monitor PolyflowExternalMonitor.cpp)
target_link_libraries(polyflow-monitor PRIVATE polyflow pthread)

# Tests
enable_testing()

add_executable(test_polyflow test_polyflow.cpp)
target_link_libraries(test_polyflow PRIVATE polyflow pthread)
add_test(NAME test_polyflow COMMAND test_polyflow)

add_executable(test_polyexec test_polyexec.cpp)
target_link_libraries(test_polyexec PRIVATE polyflow pthread)
add_test(NAME test_polyexec COMMAND test_polyexec)

# Install
install(FILES Polyflow.hpp Polyexec.hpp PolyflowData.hpp PolyflowDSLUtils.hpp
        DESTINATION include/polyflow)
install(TARGETS polyflow-monitor DESTINATION bin)
