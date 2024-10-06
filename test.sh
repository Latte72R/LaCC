#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./lcc "$input" > tmp.s
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

assert 13 "5 * 3 - 12 / (2 + 4)"
assert 2 "-8+20+2*(-5)"

echo OK