# stdlib++ headers
option(INSTALL_STDLIBPP_TO_SYSTEM_LIB "Install stdlib++ headers into system libdir" OFF)

if(INSTALL_STDLIBPP_TO_SYSTEM_LIB)
  set(_QAMRPP_STDLIBPP_DEST "${CMAKE_INSTALL_LIBDIR}/qamrpp/stdc++")
else()
  set(_QAMRPP_STDLIBPP_DEST "${QAMRPP_HOME}/stdc++")
endif()

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${_QAMRPP_STDLIBPP_DEST}
  FILES_MATCHING PATTERN "*.hpp"
)

install(
  FILES ${CMAKE_SOURCE_DIR}/lib/QaMRpp-Library.hpp
  DESTINATION ${_QAMRPP_STDLIBPP_DEST}
)
