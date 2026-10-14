#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.create_text_file("notes/todo.txt");
    zipper.create_binary_file("notes/blob.bin");
    zipper.remove_item("notes/todo.txt");
    zipper.set_destination("dist");
    zipper.set_archive_name("removed-entry.mz");

    return zipper.build().ok() ? 0 : 1;
}
