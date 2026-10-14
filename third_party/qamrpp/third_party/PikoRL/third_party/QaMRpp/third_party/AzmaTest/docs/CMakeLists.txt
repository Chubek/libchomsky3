cmake_minimum_required(VERSION 3.16)

set(AZMA_DOC_CHAPTERS
  ${CMAKE_CURRENT_SOURCE_DIR}/01-idl-foundations.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/02-idl-lexical-model.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/03-idl-declarations.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/04-idl-values-and-structures.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/05-idl-diagnostics-and-recovery.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/06-unit-framework-architecture.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/07-unit-assertions-and-fixtures.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/08-unit-cli-and-reporting.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/09-fuzz-framework-architecture.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/10-fuzz-mutation-and-corpus.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/11-fuzz-reproducibility-and-triage.rst
  ${CMAKE_CURRENT_SOURCE_DIR}/12-cross-substrate-workflows.rst
)

set(AZMA_DOCS_RENDERER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/detect_renderer.sh)
set(AZMA_DOCS_RENDER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/render_docs.sh)
set(AZMA_DOCS_RENDERER_FILE ${CMAKE_CURRENT_BINARY_DIR}/renderer.txt)
set(AZMA_DOCS_HTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
string(REPLACE ";" " " AZMA_DOC_CHAPTERS_SHELL "${AZMA_DOC_CHAPTERS}")
set(AZMA_DOC_HTML_OUTPUTS)
foreach(chapter ${AZMA_DOC_CHAPTERS})
  get_filename_component(chapter_base "${chapter}" NAME_WE)
  list(APPEND AZMA_DOC_HTML_OUTPUTS "${AZMA_DOCS_HTML_DIR}/${chapter_base}.html")
endforeach()

add_custom_command(
  OUTPUT ${AZMA_DOCS_RENDERER_FILE}
  COMMAND ${CMAKE_COMMAND} -E env sh ${AZMA_DOCS_RENDERER_SCRIPT} > ${AZMA_DOCS_RENDERER_FILE}
  DEPENDS ${AZMA_DOCS_RENDERER_SCRIPT}
  VERBATIM
)

add_custom_target(docs-detect-renderer
  DEPENDS ${AZMA_DOCS_RENDERER_FILE}
)

add_custom_target(docs
  COMMAND ${CMAKE_COMMAND} -E echo "Azma docs chapters: ${AZMA_DOC_CHAPTERS}"
  DEPENDS docs-detect-renderer ${AZMA_DOC_CHAPTERS}
  VERBATIM
)

add_custom_target(docs-render
  COMMAND sh -c "set -eu; renderer=$(cat \"${AZMA_DOCS_RENDERER_FILE}\"); sh \"${AZMA_DOCS_RENDER_SCRIPT}\" \"$renderer\" \"${AZMA_DOCS_HTML_DIR}\" ${AZMA_DOC_CHAPTERS_SHELL}"
  DEPENDS docs-detect-renderer ${AZMA_DOC_CHAPTERS} ${AZMA_DOCS_RENDER_SCRIPT}
  BYPRODUCTS ${AZMA_DOC_HTML_OUTPUTS}
  VERBATIM
)

install(FILES ${AZMA_DOC_HTML_OUTPUTS}
  DESTINATION share/doc/azmaidl
  COMPONENT docs
)
