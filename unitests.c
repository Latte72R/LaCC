
void printf();
void calloc();

int num;
char str[2];

int failures;

typedef struct B B;
struct B {
  int a;
  int b;
};

int foo_test11(int n) { return n * 4; }
int foo_test14(int *n) {
  *n = *n * 5;
  return 0;
}
int fibo_test18(int n) {
  if (n < 2)
    return n;
  else
    return fibo_test18(n - 1) + fibo_test18(n - 2);
}

int test1() { return 5 * 3 - 12 / (2 + 4); }
int test2() { return -8 + 20 + 2 * (-5); }
int test3() { return 5 + 35 % 9; }
int test4() { return 1 == (3 - 2); }
int test5() {
  int a = 5;
  int b = 2;
  return a < b;
}
int test6() { return (1 == 2) || (8 > 4); }
int test7() { return (1 != 2) && !(8 > 4); }
int test8() {
  int a = 5;
  int i;
  for (i = 0; i < 5; i++)
    a++;
  return a;
}
int test9() {
  int a = 0;
  while (a < 10)
    a++;
  return a;
}
int test10() {
  int a = 0;
  if (a < 1)
    return 2;
  else
    return 3;
}
int test11() { return foo_test11(3); }
int test12() {
  int a = 3;
  return *(&a);
}
int test13() {
  int a = 3;
  int *b = &a;
  *b += 2;
  return a;
}
int test14() {
  int a = 5;
  foo_test14(&a);
  return a;
}
int test15() { return sizeof(num); }
int test16() {
  int a[2];
  *a = 7;
  *(a + 1) = 4;
  int *p = &a;
  return *p * *(p + 1);
}
int test17() {
  int a[2];
  a[0] = 3;
  a[1] = 5;
  a[0] *= 3;
  return a[0] - a[1];
}
int test18() { return fibo_test18(9); }
int test19() {
  int a = 0;
  for (int i = 0; i < 10; i++) {
    if (i % 3 == 0)
      continue;
    a += i;
  }
  return a;
}
int test20() {
  int a = 0;
  while (a < 10) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

int test21_sub() { return num; }

int test21() {
  num = 5;
  return test21_sub();
}

int test22() {
  str[0] = 3;
  str[1] = 4;
  return str[0] * str[1];
}

int test23() {
  B *d = calloc(1, sizeof(B));
  d->a = 3;
  d->b = 7;
  return d->a * d->b;
}

void check(int result, int id, int ans) {
  if (result != ans) {
    printf("test%d failed (expected: %d / result: %d)\n", id, ans, result);
    failures++;
  }
}

int main() {
  failures = 0;
  check(test1(), 1, 13);
  check(test2(), 2, 2);
  check(test3(), 3, 13);
  check(test4(), 4, 1);
  check(test5(), 5, 0);
  check(test6(), 6, 1);
  check(test7(), 7, 0);
  check(test8(), 8, 10);
  check(test9(), 9, 10);
  check(test10(), 10, 2);
  check(test11(), 11, 12);
  check(test12(), 12, 3);
  check(test13(), 13, 5);
  check(test14(), 14, 25);
  check(test15(), 15, 4);
  check(test16(), 16, 28);
  check(test17(), 17, 4);
  check(test18(), 18, 34);
  check(test19(), 19, 27);
  check(test20(), 20, 7);
  check(test21(), 21, 5);
  check(test22(), 22, 12);
  check(test23(), 23, 21);

  if (failures == 0) {
    printf("\033[1m\033[32mAll tests passed!\033[0m\n");
  } else {
    printf("\033[1m\033[31m %d tests failed\033[0m\n", failures);
  }
  return failures;
}
