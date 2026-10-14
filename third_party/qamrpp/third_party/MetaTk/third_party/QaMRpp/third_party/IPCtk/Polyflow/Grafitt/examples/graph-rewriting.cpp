using G = grafitt::persistent_labeled_digraph<std::string, std::string>;

grafitt::rewrite::rule<std::string, std::string> r;
r.name = "InsertMediator";
r.pattern.count = 2;
r.pattern.vertex_type = "Person";
r.replacement_vertices = [](const auto&) {
    return std::vector<std::string>{"Charlie"};
};
r.replacement_edges = [](const auto& m) {
    if (m.vertices.size() < 2) return std::vector<grafitt::edge<std::string, std::string>>{};
    return std::vector<grafitt::edge<std::string, std::string>>{
        {m.vertices[0], "Charlie", "friend"},
        {"Charlie", m.vertices[1], "friend"}
    };
};
