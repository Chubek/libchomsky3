t = {10,20,30}
function iter(state, ctrl)
  i = ctrl + 1
  if i <= state.n then return i, state.t[i] end
  return nil
end
state = {t=t, n=3}
s=0
for k,v in iter, state, 0 do s = s + v end
if s==60 then print("ok19") else fail() end
