# docs/CMakeLists.txt - Documentation generation for libchomsky3

find_package(Doxygen REQUIRED)

# Set variables for Doxyfile.in substitution
set(PROJECT_NAME "libchomsky3")
set(PROJECT_VERSION "${CMAKE_PROJECT_VERSION}")
set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROJECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

# Configure Doxyfile from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    @ONLY
)

# Documentation target
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Chomsky3.i
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_SOURCE_DIR}/layout.xml
)

# Optional: Install documentation
install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
    OPTIONAL
)

# Optional: Add docs to 'all' target (uncomment if desired)
# add_dependencies(all docs)
