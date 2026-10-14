bool ok = grafitt::algo::reachable(g, std::string("Alice"), std::string("Bob"));
auto p = grafitt::algo::shortest_path(g, std::string("Alice"), std::string("Bob"));
