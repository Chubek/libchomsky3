auto q = grafitt::queryfitt::shortest_path_between("Alice", "Carol");

auto result = grafitt::queryfitt::execute(
    g,
    q,
    [](const std::string& v) { return v; },                       // name_of
    [](const std::string&, const auto&) { return true; },        // vertex predicate
    [](const auto&, const auto&) { return true; }                // edge predicate
);
