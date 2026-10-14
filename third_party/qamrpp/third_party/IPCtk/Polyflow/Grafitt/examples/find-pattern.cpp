auto q = grafitt::queryfitt::find_pattern()
    .alias_vertex("Friend")
    .alias_edge("Relationship")
    .select_n(2, "Friend")
    .edge_if(":Relationship ~ $bidir(@BestFriend)")
    .where("$only($result(1) or $result(2)) ~ $has(@Romantic)")
    .into_query("my-graphs/friendship-graph.gbin");
