cmake_minimum_required(VERSION 3.16)

set(SERDETK_DOCS
  ${CMAKE_CURRENT_SOURCE_DIR}/index.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter01-overview.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter02-data-model.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter03-formats-io.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter04-create-edit.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter05-print-minify.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter06-query-engines.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter07-validation.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter08-convert-binarize.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter09-json-reference.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter10-other-formats.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter11-sktl-itkd.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/chapter12-extensibility-testing.rst
)

add_custom_target(serdetk-docs SOURCES ${SERDETK_DOCS})
