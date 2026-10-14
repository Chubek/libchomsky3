Chapter 4 - Zipper Builder API
==============================

Subjects
--------
- zipper::make_zipper defaults
- add_file/add_directory/add_bytes/add_stream usage
- create_text_file and create_binary_file
- add_object and serializer binding
- Destination and archive naming workflow
- overwrite policy and deterministic mode
- Hook and filter attachment
- seal and reopen mutation lifecycle
- build vs build_file vs build_archive_bytes
- Manifest injection during archive finalization

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       auto zipper = minizip::api::zipper::make_zipper(
           minizip::api::speed::Balanced,
           minizip::api::focus::Compression
       );
       zipper.set_codec(minizip::backend::codec::zstd);
       zipper.add_file("README.md");
       zipper.add_bytes("generated/config.json", minizip::detail::to_bytes("{}"));
       zipper.set_destination("dist");
       zipper.set_archive_name("payload.mz");
       zipper.build();
       return 0;
   }
