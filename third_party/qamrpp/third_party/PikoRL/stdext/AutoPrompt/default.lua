return {
  name = "default",
  render = function(state)
    return (state and state.mode or "pikorl") .. "> "
  end
}
