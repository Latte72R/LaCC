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

assert 13 "5 * 3 - 12 / (2 + 4);"
assert 2 "-8+20+2*(-5);"
assert 1 "1 == 3 - 2;"
assert 5 "a = 5; a;"
assert 3 "a = 5; b = 2; a - b;"
assert 0 "a = 5; b = 2; a < b;"
assert 10 "a = 5; for (i = 0; i < 5; i = i + 1) a = a + 1; a;"
assert 10 "a = 0; while (a < 10) a = a + 1; a;"
assert 2 "a = 0; if (a < 1) { return 2; } else { return 3; }"

echo OK
