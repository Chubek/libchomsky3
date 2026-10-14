add_library(SerdeTkXPath STATIC
    Adapter.cpp
    CodeDAG.cpp
    CodeTree.cpp
    DataModel.cpp
    Evalutator.cpp
    Hooks.cpp
    Parser.cpp
    PrettyPrint.cpp
    Tokenizer.cpp
    Validator.cpp)
target_include_directories(SerdeTkXPath PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(SerdeTkXPath PUBLIC SerdeTk)
target_compile_features(SerdeTkXPath PUBLIC cxx_std_20)
