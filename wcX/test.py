for c in range(1, 1001):
    for b in range(1, c):
        a = 1000 - b - c
        if a < 0:
            continue
        if a * a + b * b == c * c:
            print(a * b * c, a, b, c)
