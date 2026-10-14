set(PIKORL_EXTENSION_MANIFEST
    "${CMAKE_CURRENT_LIST_DIR}/../manifests/Extensions.yaml")

if(NOT EXISTS "${PIKORL_EXTENSION_MANIFEST}")
  message(FATAL_ERROR
    "Extension manifest is missing: ${PIKORL_EXTENSION_MANIFEST}")
endif()

install(FILES "${PIKORL_EXTENSION_MANIFEST}"
        DESTINATION "stdlib")
