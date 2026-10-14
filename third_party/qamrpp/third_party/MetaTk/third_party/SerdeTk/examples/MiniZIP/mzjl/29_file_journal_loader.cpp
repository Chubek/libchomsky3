#include "MiniZIP.hpp"

int main() {
    auto result = minizip::api::make_journal_archiver(
        "exmaples/MiniZIP/mzjl/31_journal_manifest.mzjl",
        minizip::api::MZJL::FromFile
    )
        .lint()
        .parse()
        .validate()
        .scaffold()
        .build()
        .save();

    return result.ok() ? 0 : 1;
}
