function it(s,c)
  n = c + 1
  if n <= 3 then return n end
  return nil
end
s=0
for k in it, {}, 0 do s = s + k end
if s==6 then print("ok20") else fail() end
