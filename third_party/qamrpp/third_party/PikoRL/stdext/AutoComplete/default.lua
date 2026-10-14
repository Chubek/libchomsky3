return {
  name = "default",
  keywords = { "help", "load", "quit", "set" },
  suggest = function(prefix)
    local results = {}
    for _, keyword in ipairs({ "help", "load", "quit", "set" }) do
      if keyword:sub(1, #prefix) == prefix then table.insert(results, keyword) end
    end
    return results
  end
}
