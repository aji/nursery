# computes the nth fibonacci number the dumb way
fn fib (n)
  (n < 2) ? n : fib (n - 1) + fib (n - 2)

# much faster fibonacci calculator
fn fib2_ (n a b) (n == 0) ? a : fib2_ (n-1 b a+b)
fn fib2 (n) fib2_ (n 0 1)
# equivalent erlang, for those who can't read P yet
# fib2_(0, A, _) -> A;
# fib2_(N, A, B) -> fib2(N-1, B, A+B).
# fib(N) -> fib2_(N, 0, 1).
