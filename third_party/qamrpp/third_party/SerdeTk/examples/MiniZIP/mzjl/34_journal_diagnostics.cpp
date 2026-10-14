#include "MiniZIP.hpp"

#include <cstddef>

int main() {
    auto archiver = minizip::api::make_journal_archiver(
        "exmaples/MiniZIP/mzjl/33_journal_with_exclude_and_filter.mzjl",
        minizip::api::MZJL::FromFile
    )
        .lint()
        .parse()
        .validate();

    return archiver.ok() ? 0 : static_cast<int>(archiver.diagnostics().size() > 0);
}
