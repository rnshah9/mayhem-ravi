function f()
    return 1,2,3
end

local a,b,c,d = f()
print(a,b,c,d)
assert(a==1 and b==2 and c==3 and d==nil)

print '66 Ok'