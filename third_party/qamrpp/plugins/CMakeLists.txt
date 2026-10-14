set(_QAMRPP_PLUGIN_DEST "${QAMRPP_HOME}/plugins")

# Jiterati include paths needed by jit-backend and aot-backend
set(JITERATI_ROOT "${CMAKE_SOURCE_DIR}/third_party/Jiterati")
if(EXISTS "${JITERATI_ROOT}/include/Jiterati.hpp")
    set(JITERATI_INCLUDE_DIRS
        "${JITERATI_ROOT}/include"
        "${JITERATI_ROOT}"
    )
    message(STATUS "Jiterati found at ${JITERATI_ROOT}")
else()
    set(JITERATI_INCLUDE_DIRS "")
    message(WARNING "Jiterati not found; JIT/AOT plugins will need manual include paths")
endif()

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${_QAMRPP_PLUGIN_DEST}
  PATTERN "CMakeLists.txt" EXCLUDE
  PATTERN "MANIFEST.yaml" EXCLUDE
)

# Install MANIFEST
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.yaml
  DESTINATION ${_QAMRPP_PLUGIN_DEST}
)

# Export Jiterati include paths so consumers can use them
set(QAMRPP_JITERATI_INCLUDE_DIRS "${JITERATI_INCLUDE_DIRS}" PARENT_SCOPE)
