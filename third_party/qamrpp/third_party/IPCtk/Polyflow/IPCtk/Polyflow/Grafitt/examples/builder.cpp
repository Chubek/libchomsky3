using G = grafitt::imperative_graph<std::string, std::string>;

auto g = grafitt::builder::imperative_builder<G>(grafitt::direction::directed)
    .vertex("Alice")
    .vertex("Bob")
    .edge("Alice", "Bob", "friend")
    .build();
