# stdlib runtime modules
set(_QAMRPP_STDLIB_LIBDIR "${QAMRPP_HOME}/lib/stdlua")

function(add_qamrpp_stdlib name)
  add_library(qamrpp_${name} SHARED ${name}.c $<TARGET_OBJECTS:qamrpp_library>)
  set_target_properties(qamrpp_${name} PROPERTIES PREFIX "libqamrpp_" OUTPUT_NAME "${name}")
  target_include_directories(qamrpp_${name} PRIVATE ${CMAKE_SOURCE_DIR}/qlib/C)
  install(TARGETS qamrpp_${name} LIBRARY DESTINATION ${_QAMRPP_STDLIB_LIBDIR}
          RUNTIME DESTINATION ${_QAMRPP_STDLIB_LIBDIR})
endfunction()

add_qamrpp_stdlib(core)
add_qamrpp_stdlib(string)
add_qamrpp_stdlib(table)
add_qamrpp_stdlib(math)
add_qamrpp_stdlib(io)
add_qamrpp_stdlib(os)
add_qamrpp_stdlib(debug)
add_qamrpp_stdlib(coroutine)
add_qamrpp_stdlib(package)
add_qamrpp_stdlib(utf8)
