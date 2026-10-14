return {
  name = "plain",
  theme = {
    comment = "bright-black",
    string = "green",
    number = "magenta",
    operator = "white",
    identifier = "default"
  },
  patterns = {
    comment = "#.*$",
    string = "\"([^\"\\]|\\.)*\"|'([^'\\]|\\.)*'",
    number = "%f[%d]%d+%.?%d*%f[^%d]",
    operator = "[+%-%*/=<>!&|%^~]+",
    identifier = "[_%a][_%w]*"
  },
  highlight = function(line)
    return { { token = "text", value = line or "" } }
  end
}
