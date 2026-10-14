mt = {__add = function(a,b) return a.v + b.v end}
a = {v=2}
b = {v=5}
setmetatable(a, mt)
setmetatable(b, mt)
if a + b == 7 then print("ok23") else fail() end
