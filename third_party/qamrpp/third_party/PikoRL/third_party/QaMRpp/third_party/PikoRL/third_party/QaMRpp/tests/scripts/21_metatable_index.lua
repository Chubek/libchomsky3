t = {}
mt = {__index = {x = 99}}
setmetatable(t, mt)
if t.x == 99 then print("ok21") else fail() end
