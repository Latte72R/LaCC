#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 13 "main() { return 5 * 3 - 12 / (2 + 4); }"
assert 2 "main() { return -8 + 20 + 2 * (-5); }"
assert 1 "main() { return 1 == 3 - 2; }"
assert 0 "main() { a = 5; b = 2; return a < b; }"
assert 10 "main() { a = 5; for (i = 0; i < 5; i = i + 1) a = a + 1; return a; }"
assert 10 "main() { a = 0; while (a < 10) a = a + 1; return a;}"
assert 2 "main() { a = 0; if (a < 1) { return 2; } else { return 3; } }"
assert 12 "foo(n) { return n * 4; } main() { return foo(3); }"
assert 13 """
fibo(n) {
  if (n < 2) {
    return n;
  } else {
    return fibo(n - 1) + fibo(n - 2);
  }
main() {
  return fibo(7);
}
"""

echo OK
