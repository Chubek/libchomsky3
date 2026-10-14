sink = {}
mt = {__newindex = sink}
t = {}
setmetatable(t, mt)
t.a = 12
if sink.a == 12 then print("ok22") else fail() end
