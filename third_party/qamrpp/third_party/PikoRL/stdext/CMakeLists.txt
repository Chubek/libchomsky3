set(PIKORL_STDLIB_INSTALL_DIR "stdlib" CACHE STRING
    "Standard-library directory relative to PIKORL_HOME")

foreach(extension AutoComplete AutoSyntax AutoCommand AutoPrompt)
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${extension}/"
    DESTINATION "${PIKORL_STDLIB_INSTALL_DIR}/${extension}"
  )
endforeach()
