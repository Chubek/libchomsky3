# HandleDocs.cmake
# Centralized docs wiring and renderer detection.

if(NOT DEFINED AZMA_BUILD_DOCS)
  option(AZMA_BUILD_DOCS "Build Azma documentation" ON)
endif()

if(NOT AZMA_BUILD_DOCS)
  return()
endif()

set(AZMA_DOCS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../docs")
set(AZMA_DOCS_BUILD_DIR "${CMAKE_BINARY_DIR}/docs")
set(AZMA_DOCS_RENDERER_SCRIPT "${AZMA_DOCS_SOURCE_DIR}/detect_renderer.sh")

if(NOT EXISTS "${AZMA_DOCS_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "docs/CMakeLists.txt is required when AZMA_BUILD_DOCS=ON")
endif()

add_subdirectory("${AZMA_DOCS_SOURCE_DIR}" "${AZMA_DOCS_BUILD_DIR}")
