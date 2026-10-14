add_executable(qamrpp-cli qamrpp-cli.cpp)
set_target_properties(qamrpp-cli PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(qamrpp-assembler qamrpp-assembler.cpp)
set_target_properties(qamrpp-assembler PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(qamrpp-linker qamrpp-linker.cpp)
set_target_properties(qamrpp-linker PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

install(TARGETS qamrpp-cli RUNTIME DESTINATION bin)
install(TARGETS qamrpp-assembler RUNTIME DESTINATION bin)
install(TARGETS qamrpp-linker RUNTIME DESTINATION bin)
