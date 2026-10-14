#ifndef QAMRPP_QBF_HPP
#define QAMRPP_QBF_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace qamrpp {

struct QBFEntry {
    std::string path;
    std::string source;
};

inline bool write_qbf(const std::string& out_path, const std::vector<QBFEntry>& entries) {
    std::ofstream out(out_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.good()) return false;
    out << "QBF1\n" << entries.size() << "\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << entries[i].path.size() << " " << entries[i].source.size() << "\n";
        out << entries[i].path;
        out << entries[i].source;
    }
    return true;
}

inline bool read_qbf(const std::string& in_path, std::vector<QBFEntry>& entries) {
    std::ifstream in(in_path.c_str(), std::ios::binary);
    if (!in.good()) return false;
    std::string magic;
    std::getline(in, magic);
    if (magic != "QBF1") return false;
    size_t count = 0;
    in >> count;
    in.get();
    entries.clear();
    for (size_t i = 0; i < count; ++i) {
        size_t p = 0, s = 0;
        in >> p >> s;
        in.get();
        QBFEntry e;
        e.path.resize(p);
        e.source.resize(s);
        in.read(&e.path[0], static_cast<std::streamsize>(p));
        in.read(&e.source[0], static_cast<std::streamsize>(s));
        entries.push_back(e);
    }
    return true;
}

} // namespace qamrpp

#endif
