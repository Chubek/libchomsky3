#include "grafitt.hpp"
#include <iostream>
#include <string>

int main() {
    grafitt::imperative_graph<std::string, std::string> g(grafitt::direction::directed);
    g.add_edge("Alice", "Bob", "friend");
    g.add_edge("Bob", "Carol", "best-friend");

    std::cout << "Vertices: " << g.nb_vertex() << "\n";
    std::cout << "Edges: " << g.nb_edges() << "\n";
}
