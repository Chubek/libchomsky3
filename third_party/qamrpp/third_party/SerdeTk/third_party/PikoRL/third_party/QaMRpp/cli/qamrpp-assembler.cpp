#include <glob.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../plugins/QaMRpp-QBF.hpp"

static std::vector<std::string> expand_glob(const std::string& pat) {
    glob_t g;
    std::vector<std::string> out;
    if (glob(pat.c_str(), 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) out.push_back(g.gl_pathv[i]);
    }
    globfree(&g);
    return out;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: qamrpp-assembler <out.qbf> <glob...>\n";
        return 1;
    }
    std::vector<qamrpp::QBFEntry> entries;
    for (int i = 2; i < argc; ++i) {
        std::vector<std::string> files = expand_glob(argv[i]);
        for (size_t j = 0; j < files.size(); ++j) {
            std::ifstream in(files[j].c_str(), std::ios::binary);
            if (!in.good()) continue;
            std::ostringstream ss;
            ss << in.rdbuf();
            qamrpp::QBFEntry e;
            e.path = files[j];
            e.source = ss.str();
            entries.push_back(e);
        }
    }
    if (!qamrpp::write_qbf(argv[1], entries)) return 2;
    return 0;
}
