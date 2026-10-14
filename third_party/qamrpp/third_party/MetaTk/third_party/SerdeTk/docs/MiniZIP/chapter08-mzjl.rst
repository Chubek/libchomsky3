Chapter 8 - MZJL Journal Workflow
=================================

Subjects
--------
- Journal-driven archive planning
- make_journal_archiver sources: file vs string
- lint, parse, validate, scaffold, build, save chain
- Real DSLtk parser combinator integration
- Supported commands: archive, source, manifest, manifest_cmd
- Supported commands: level and save_to
- Current treatment of exclude and filter directives
- Journal diagnostics model
- Scaffolding journals into zipper operations
- Practical authoring constraints for deterministic journals

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       std::string journal = R"(
   %MZJL-v1.0.0
   archive "inline.mz"
   source "README.md"
   manifest INIManifest
   manifest_cmd ".section package"
   manifest_cmd ".kv name MiniZIP"
   save_to "dist"
   )";

       auto result = minizip::api::make_journal_archiver(journal, minizip::api::MZJL::FromString)
           .lint()
           .parse()
           .validate()
           .scaffold()
           .build()
           .save();
       return result.ok() ? 0 : 1;
   }
