# Documentation generation — gated by -DGENERATE_DOCS=ON.
# Produces an API reference by running Doxygen on Doxyfile.in.

find_package(Doxygen)

if(GENERATE_DOCS)
  if(NOT DOXYGEN_FOUND)
    message(WARNING "GENERATE_DOCS=ON but Doxygen was not found; skipping docs.")
    return()
  endif()

  set(DOXYFILE_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

  add_custom_target(docs ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Satie API documentation with Doxygen"
    VERBATIM)

  install(DIRECTORY ${CMAKE_BINARY_DIR}/docs/doxygen/html/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    OPTIONAL)
else()
  add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E echo
      "Set -DGENERATE_DOCS=ON to build documentation.")
endif()
