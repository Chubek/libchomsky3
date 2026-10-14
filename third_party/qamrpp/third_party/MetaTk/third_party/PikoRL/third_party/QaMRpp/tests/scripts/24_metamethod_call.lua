t = {}
mt = {__call = function(self,x,y) return x+y end}
setmetatable(t, mt)
if t(3,6)==9 then print("ok24") else fail() end
