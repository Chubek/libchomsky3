add_library(pikorl-example-plugin MODULE hello-plugin.cpp)
target_compile_features(pikorl-example-plugin PRIVATE cxx_std_17)
target_include_directories(pikorl-example-plugin PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include")
set_target_properties(pikorl-example-plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "hello-plugin")
