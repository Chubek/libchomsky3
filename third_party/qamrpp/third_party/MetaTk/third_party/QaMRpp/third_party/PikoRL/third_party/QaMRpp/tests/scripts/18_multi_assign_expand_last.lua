function tri() return 4,5,6 end
a,b,c = 1, tri()
if a==1 and b==4 and c==5 then print("ok18") else fail() end
