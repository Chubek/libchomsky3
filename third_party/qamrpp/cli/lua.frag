qamrpp_cli_keywords = {
  "and", "break", "do", "else", "elseif", "end", "false", "for",
  "function", "if", "in", "local", "nil", "not", "or", "repeat",
  "return", "then", "true", "until", "while", "exit", "quit"
}

qamrpp_cli_directives = {
  "%help", "%autocomplete", "%color", "%history", "%prompt", "%syntax",
  "%load", "%require", "%run", "%source", "%dump", "%serialize",
  "%compile-c", "%compile-wasm", "%pwd", "%cd", "%home", "%libs",
  "%scripts", "%qbf", "%clear", "%reset", "%status", "%quit"
}

function lpicorl_completion(prefix)
  local out = {}
  if not prefix then
    return out
  end
  local plen = #prefix
  for i = 1, #qamrpp_cli_keywords do
    local word = qamrpp_cli_keywords[i]
    if string.sub(word, 1, plen) == prefix then
      out[#out + 1] = word
    end
  end
  for i = 1, #qamrpp_cli_directives do
    local word = qamrpp_cli_directives[i]
    if string.sub(word, 1, plen) == prefix then
      out[#out + 1] = word
    end
  end
  return out
end

function lpicorl_help()
  return "QaMRpp CLI helpers loaded"
end
