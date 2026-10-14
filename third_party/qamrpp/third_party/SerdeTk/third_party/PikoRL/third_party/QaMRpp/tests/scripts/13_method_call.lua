obj = {v=10}
obj.inc = function(self,n) self.v = self.v + n return self.v end
if obj:inc(5)==15 and obj.v==15 then print("ok13") else fail() end
