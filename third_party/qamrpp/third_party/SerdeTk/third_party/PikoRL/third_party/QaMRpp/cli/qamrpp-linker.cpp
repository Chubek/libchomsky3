#include <iostream>
#include <string>
#include <vector>

#include "../plugins/QaMRpp-QBF.hpp"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: qamrpp-linker <out.qbf> <in1.qbf> <in2.qbf...>\n";
        return 1;
    }
    std::vector<qamrpp::QBFEntry> merged;
    for (int i = 2; i < argc; ++i) {
        std::vector<qamrpp::QBFEntry> entries;
        if (!qamrpp::read_qbf(argv[i], entries)) {
            std::cerr << "failed to read: " << argv[i] << "\n";
            return 2;
        }
        merged.insert(merged.end(), entries.begin(), entries.end());
    }
    if (!qamrpp::write_qbf(argv[1], merged)) return 3;
    return 0;
}
