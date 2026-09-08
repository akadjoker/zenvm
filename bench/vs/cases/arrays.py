a = []
i = 0
while i < 200000:
    a.append(i)
    i = i + 1
s = 0
k = 0
while k < 10:
    i = 0
    while i < 200000:
        s = s + a[i]
        i = i + 1
    k = k + 1
print(s)
