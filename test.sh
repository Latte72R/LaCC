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

assert 13 "int main() { return 5 * 3 - 12 / (2 + 4); }"
assert 2 "int main() { return -8 + 20 + 2 * (-5); }"
assert 13 "int main() { return 5 + 35 % 9; }"
assert 1 "int main() { return 1 == 3 - 2; }"
assert 0 "int main() { int a = 5; int b = 2; return a < b; }"
assert 1 "int main() { return 1 == 2 || 8 > 4; }"
assert 0 "int main() { return (1 != 2) && !(8 > 4); }"
assert 10 "int main() { int a = 5; int i; for (i = 0; i < 5; i = i + 1) a = a + 1; return a; }"
assert 10 "int main() { int a = 0; while (a < 10) a = a + 1; return a;}"
assert 2 "int main() { int a = 0; if (a < 1) { return 2; } else { return 3; } }"
assert 12 "int foo(int n) { return n * 4; } int main() { return foo(3); }"
assert 3 "int main() { int a = 3; return *(&a); }"
assert 5 "int main() { int a = 3; int *b = &a; *b = 5; return a; }"
assert 25 "int foo(int *n) { *n = *n * 5; return 0;} int main() { int a = 5; foo(&a); return a;}"
assert 4 "int main() { int a = 3; return sizeof(a); }"
assert 28 "int main() { int a[2]; *a = 7; *(a + 1) = 4; int *p = &a; return *p * *(p + 1); }"
assert 4 "int main() { int a[2]; a[0] = 9; a[1] = 5; return a[0] - a[1]; }"
assert 13 """int fibo(int n) { if (n < 2) { return n; } else { return fibo(n - 1) + fibo(n - 2); } }
int main() { return fibo(7); }"""
assert 27 "int main() { int a = 0; int i; for (i = 0; i < 10; i = i + 1) { if (i % 3 == 0) continue; a = a + i; } return a; }"
assert 7 "int main() { int a = 0; while (a < 10) { if (a > 6) break; a = a + 1; } return a;}"

echo OK
