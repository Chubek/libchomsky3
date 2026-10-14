#include "MiniZIP.hpp"

#include <string>

int main() {
    std::string journal = R"(%MZJL-v1.0.0
archive "inline-journal.mz"
source "README.md"
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
