Chapter 6 - Middleware Model
============================

Subjects
--------
- Serialization middleware contract
- Deserialization middleware contract
- Filter middleware ordering and reversal
- Transport middleware responsibilities
- Hook middleware lifecycle callbacks
- Default filesystem transport behavior
- Per-archive middleware composition
- Object archiving through std::any type erasure
- Metadata tagging for serializers and filters
- Extension guidance for custom middleware

Example
-------
.. code-block:: cpp

   #include "MiniZIP.hpp"

   int main() {
       minizip::api::filter identity{
           .tag = "identity",
           .encode = [](std::span<const std::byte> bytes) {
               return minizip::api::result<minizip::detail::byte_vector>(
                   minizip::detail::byte_vector(bytes.begin(), bytes.end())
               );
           },
           .decode = [](std::span<const std::byte> bytes) {
               return minizip::api::result<minizip::detail::byte_vector>(
                   minizip::detail::byte_vector(bytes.begin(), bytes.end())
               );
           }
       };

       auto zipper = minizip::api::zipper::make_zipper();
       zipper.with_filter(identity);
       zipper.add_bytes("x.bin", minizip::detail::to_bytes("abc"));
       zipper.build_file("x.mz");
       return 0;
   }
