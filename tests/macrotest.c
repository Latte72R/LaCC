void printf();
int strcmp();

int failures;
int test_cnt;

#define CONST42 42
int test1() { return CONST42; }

#define SQUARE(x) ((x) * (x))
int test2() { return SQUARE(5); }

#define STRIZE(x) #x
int test3() {
  char *s = STRIZE(foo + bar);
  return strcmp(s, "foo + bar");
}

#define JOIN(a, b) a##b
#define MAKE_COUNTER(n) JOIN(counter_, n)
int test4() {
  int counter_7 = 21;
  return MAKE_COUNTER(7);
}

#define INC(x) ((x) + 1)
#define DOUBLE(x) ((x) * 2)
#define APPLY(fn, arg) fn(arg)
int test5() { return DOUBLE(APPLY(INC, 9)); }

#define FLAG_TRUE 1
#if FLAG_TRUE
int test6() { return 123; }
#else
int test6() { return -1; }
#endif

#define FLAG_FALSE 0
#if FLAG_FALSE
int test7() { return -1; }
#elif defined(CONST42)
int test7() { return CONST42; }
#else
int test7() { return -2; }
#endif

#define VAL 1 + 2
#if VAL * 3 == 9
int test8() { return 0; }
#else
int test8() { return 1; }
#endif

int test9() {
#if 1
#if 0
  return 0;
#else
  return 7;
#endif
#else
  return -1;
#endif
}

#if defined(UNDEFINED_MACRO)
int test10() { return 0; }
#else
int test10() { return 1; }
#endif

#ifdef CONST42
int test11() { return 11; }
#else
int test11() { return -11; }
#endif

#ifndef NOT_DEFINED_MACRO
int test12() { return 12; }
#else
int test12() { return -12; }
#endif

#define TO_REMOVE 13
#ifdef TO_REMOVE
#undef TO_REMOVE
#endif

#ifdef TO_REMOVE
int test13() { return -13; }
#else
int test13() { return 13; }
#endif

void check(int result, int id, int expected) {
  test_cnt++;
  if (result != expected) {
    printf("macro test%d failed (expected: %d / result: %d)\n", id, expected, result);
    failures++;
  }
}

int main() {
  failures = 0;
  test_cnt = 0;

  check(test1(), 1, 42);
  check(test2(), 2, 25);
  check(test3(), 3, 0);
  check(test4(), 4, 21);
  check(test5(), 5, 20);
  check(test6(), 6, 123);
  check(test7(), 7, 42);
  check(test8(), 8, 1);
  check(test9(), 9, 7);
  check(test10(), 10, 1);
  check(test11(), 11, 11);
  check(test12(), 12, 12);
  check(test13(), 13, 13);

  if (failures == 0) {
    printf("\033[1;36mAll %d macro tests passed\033[0m\n", test_cnt);
  } else {
    printf("\033[1;31m%d of %d macro tests failed\033[0m\n", failures, test_cnt);
  }

  return failures;
}
