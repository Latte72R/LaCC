
void printf();

int num;
char str[2];

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
  int i;
  for (i = 0; i < 10; i++) {
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
void test21() { num = 5; }
int test22() {
  str[0] = 3;
  str[1] = 4;
  return str[0] * str[1];
}

int main() {
  int failures = 0;
  int result;

  result = test1();
  if (result != 13) {
    printf("test1 failed (expected: 13 / result: %d)\n", result);
    failures++;
  }
  result = test2();
  if (result != 2) {
    printf("test2 failed (expected: 2 / result: %d)\n", result);
    failures++;
  }
  result = test3();
  if (result != 13) {
    printf("test3 failed (expected: 13 / result: %d)\n", result);
    failures++;
  }
  result = test4();
  if (result != 1) {
    printf("test4 failed (expected: 1 / result: %d)\n", result);
    failures++;
  }
  result = test5();
  if (result != 0) {
    printf("test5 failed (expected: 0 / result: %d)\n", result);
    failures++;
  }
  result = test6();
  if (result != 1) {
    printf("test6 failed (expected: 1 / result: %d)\n", result);
    failures++;
  }
  result = test7();
  if (result != 0) {
    printf("test7 failed (expected: 0 / result: %d)\n", result);
    failures++;
  }
  result = test8();
  if (result != 10) {
    printf("test8 failed (expected: 10 / result: %d)\n", result);
    failures++;
  }
  result = test9();
  if (result != 10) {
    printf("test9 failed (expected: 10 / result: %d)\n", result);
    failures++;
  }
  result = test10();
  if (result != 2) {
    printf("test10 failed (expected: 2 / result: %d)\n", result);
    failures++;
  }
  result = test11();
  if (result != 12) {
    printf("test11 failed (expected: 12 / result: %d)\n", result);
    failures++;
  }
  result = test12();
  if (result != 3) {
    printf("test12 failed (expected: 3 / result: %d)\n", result);
    failures++;
  }
  result = test13();
  if (result != 5) {
    printf("test13 failed (expected: 5 / result: %d)\n", result);
    failures++;
  }
  result = test14();
  if (result != 25) {
    printf("test14 failed (expected: 25 / result: %d)\n", result);
    failures++;
  }
  result = test15();
  if (result != 4) {
    printf("test15 failed (expected: 4 / result: %d)\n", result);
    failures++;
  }
  result = test16();
  if (result != 28) {
    printf("test16 failed (expected: 28 / result: %d)\n", result);
    failures++;
  }
  result = test17();
  if (result != 4) {
    printf("test17 failed (expected: 4 / result: %d)\n", result);
    failures++;
  }
  result = test18();
  if (result != 34) {
    printf("test18 failed (expected: 34 / result: %d)\n", result);
    failures++;
  }
  result = test19();
  if (result != 27) {
    printf("test19 failed (expected: 27 / result: %d)\n", result);
    failures++;
  }
  result = test20();
  if (result != 7) {
    printf("test20 failed (expected: 7 / result: %d)\n", result);
    failures++;
  }
  test21();
  result = num;
  if (result != 5) {
    printf("test21 failed (expected: 5 / result: %d)\n", result);
    failures++;
  }
  result = test22();
  if (result != 12) {
    printf("test22 failed (expected: 12 / result: %d)\n", result);
    failures++;
  }

  if (failures == 0) {
    printf("\033[1m\033[32mAll tests passed!\033[0m\n");
  } else {
    printf("\033[1m\033[31m %d tests failed\033[0m\n", failures);
  }
  return failures;
}
