class P:
    def __init__(self):
        self.x = 1
        self.y = 2
    def step(self):
        self.x = self.x + self.y
p = P()
i = 0
while i < 2000000:
    p.step()
    i = i + 1
print(p.x)
