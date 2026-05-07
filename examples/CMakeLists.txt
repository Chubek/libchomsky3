cmake_minimum_required(VERSION 3.15)

# ============================================================================
# Example Programs
# ============================================================================

# Helper function to create example executables
function(add_chomsky3_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE chomsky3)
    
    # Set output directory
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
    )
    
    # Optional: Install examples
    install(TARGETS ${name}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
        COMPONENT examples
    )
endfunction()

# ============================================================================
# Define Examples
# ============================================================================

add_chomsky3_example(email-validation email-validation.c)
add_chomsky3_example(jit-compilation jit-compilation.c)
add_chomsky3_example(resource-limits resource-limits.c)
add_chomsky3_example(unicode-character-lookup unicode-chatacter-lookup.c)
add_chomsky3_example(url-parsing url-parsing.c)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Configured ${CMAKE_CURRENT_SOURCE_DIR}:")
message(STATUS "  - email-validation")
message(STATUS "  - jit-compilation")
message(STATUS "  - resource-limits")
message(STATUS "  - unicode-character-lookup")
message(STATUS "  - url-parsing")
