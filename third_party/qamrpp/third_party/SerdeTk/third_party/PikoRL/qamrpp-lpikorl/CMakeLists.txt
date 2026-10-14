set(PICORL_QAMRPP_LPIKORL_SOURCES
  lpikorl.c
  prompt.c
  syntax.c
  directive.c
  completion.c
  help.c
  history.c
  idl.c
  macros.c
  menu.c
  page.c
  sandboxing.c
  theme.c
  colors.c
  web.c
)

add_library(pikorl_lpikorl STATIC
  ${PICORL_QAMRPP_LPIKORL_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/QaMRpp/QaMRpp-Library.c
)

target_include_directories(pikorl_lpikorl
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/QaMRpp
)

target_compile_features(pikorl_lpikorl PUBLIC c_std_99)
