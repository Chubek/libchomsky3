#include "MiniZIP.hpp"

#include <sstream>

int main() {
    std::istringstream log_stream("line 1\nline 2\nline 3\n");

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_stream("logs/latest.txt", log_stream, "text/plain");
    zipper.set_destination("dist");
    zipper.set_archive_name("streamed.mz");

    return zipper.build().ok() ? 0 : 1;
}
