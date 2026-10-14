add_library(pikorl-interop INTERFACE)
target_include_directories(pikorl-interop INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/QaMRpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/S7Scheme")
