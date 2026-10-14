# CLI / REPL — standard library only, no external deps.

add_executable(satie-cli satie-cli.cpp)
target_link_libraries(satie-cli PRIVATE satie::satie)

add_executable(satie-repl satie-repl.cpp)
target_link_libraries(satie-repl PRIVATE satie::satie)

install(TARGETS satie-cli satie-repl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES Satie.syn
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie)
