add_library(SerdeTkSemver STATIC
    DepndencyGraph.cpp Parser.cpp Resolver.cpp Semver.cpp Storage.cpp)
target_include_directories(SerdeTkSemver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party/Satie/include
    ${CMAKE_BINARY_DIR}/generated ${CMAKE_SOURCE_DIR}/third_party/lemon)
target_link_libraries(SerdeTkSemver PUBLIC SerdeTk SerdeTkSatie)
target_compile_features(SerdeTkSemver PUBLIC cxx_std_20)
