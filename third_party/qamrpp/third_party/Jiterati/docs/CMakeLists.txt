# Documentation build (Doxygen)
find_package(Doxygen QUIET)

set(DOCS_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(DOCS_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})

if(DOXYGEN_FOUND)
    # Source directories to index
    set(DOXYGEN_INPUT_DIRS
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/IR"
        "${CMAKE_SOURCE_DIR}/BE"
        "${CMAKE_SOURCE_DIR}/Passlib"
        "${CMAKE_SOURCE_DIR}/Plugins"
        "${CMAKE_SOURCE_DIR}/docs"
        "${CMAKE_SOURCE_DIR}/specs"
    )

    string(REPLACE ";" " " DOXYGEN_INPUT "${DOXYGEN_INPUT_DIRS}")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating Jiterati API and manual documentation"
        VERBATIM
    )
else()
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E echo
                "Doxygen not found; install Doxygen to build documentation."
        VERBATIM
    )
endif()
