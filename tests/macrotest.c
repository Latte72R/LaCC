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

  if (failures == 0) {
    printf("\033[1;36mAll %d macro tests passed\033[0m\n", test_cnt);
  } else {
    printf("\033[1;31m%d of %d macro tests failed\033[0m\n", failures, test_cnt);
  }

  return failures;
}
