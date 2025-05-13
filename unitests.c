
// This is a comment.

void printf();
void *calloc();

int num;
char str[2];

int failures;

typedef struct STRUCT STRUCT;
struct STRUCT {
  int a;
  int b[2];
};

typedef enum { A, B, C } ENUM;

typedef struct ST28 ST28;
struct ST28 {
  ST28 *next;
  char ch;
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

int test21() {
  char *s = "hello";
  return s[0];
}

int test22() {
  str[0] = 3;
  str[1] = 4;
  return str[0] * str[1];
}

int test23() {
  STRUCT *c = calloc(1, sizeof(STRUCT));
  c->a = 5;
  c->b[0] = 1;
  c->b[1] = 6;
  return c->a * (c->b[0] + c->b[1]);
}

ENUM test24() {
  ENUM a = C;
  return a;
}

char test25() { return 'a'; }

int test26() {
  int a = 0;
  int i = 0;
  while (i < 10) {
    i++;
    if (i % 3 == 0)
      continue;
    a += i;
  }
  return a;
}

int test27() {
  int a = 2;
  int b = a++;
  a++;
  return a * b;
}

int test28() {
  ST28 *a = calloc(1, sizeof(ST28));
  ST28 *b = calloc(1, sizeof(ST28));
  ST28 *c = calloc(1, sizeof(ST28));
  c->ch = 'C';
  a->next = b;
  b->next = c;
  return a->next->next->ch;
}

int test29() {
  int a = 2;
  int b = ++a;
  ++a;
  return a * b;
}

int test30() {
  int a = 0;
  for (;;) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

int test31() {
  int a;
  return (a = 7);
}

int test32() {
  // ビットAND
  return 6 & 3; // 0b110 & 0b011 = 0b010 = 2
}

int test33() {
  // ビットOR
  return 6 | 3; // 0b110 | 0b011 = 0b111 = 7
}

int test34() {
  // ビットXOR
  return 6 ^ 3; // 0b110 ^ 0b011 = 0b101 = 5
}

int test35() {
  // ビットNOT
  return ~5; // ~0b0101 = ...11111010 = -6
}

int test36() {
  // 左シフト
  return 1 << 4; // 0b1 << 4 = 0b10000 = 16
}

int test37() {
  // 右シフト
  return 16 >> 2; // 0b10000 >> 2 = 0b100 = 4
}

int test38() {
  STRUCT c;
  c.a = 5;
  ((&c)->b)[0] = 3;
  c.b[1] = 4;
  return c.a * (&c)->b[0] - (&c)->b[1];
}

int test39() {
  // 演算子の優先順位
  return 1 + 2 * 3 << 2;
}

int test40() {
  // ポインタ差：配列要素間の差
  char arr[3];
  return &arr[2] - &arr[0]; // 2
}

int test41() {
  // 構造体のサイズ
  // STRUCT は int(4) + int の合計 12 バイト
  return sizeof(STRUCT);
}

int test42() {
  // 演算子の優先順位
  return 1 + ~2;
}

int test43() {
  // 演算子の優先順位
  return 1 & 2 == 2;
}

int test44() {
  int n = 1;
  for (int i = 1; i < 4; i++) {
    n *= i;
  }
  for (int i = 1; i < 4; i++) {
    n *= i;
  }
  return n;
}

int test45() {
  int a = 2, b = 3, c = 5;
  return a * b + c;
}

int test46() {
  int arr[2][2];
  arr[0][0] = 2;
  arr[0][1] = 4;
  arr[1][0] = 6;
  arr[1][1] = 8;
  return arr[0][0] * arr[0][1] + arr[1][0] * arr[1][1];
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
  check(test21(), 21, 104);
  check(test22(), 22, 12);
  check(test23(), 23, 35);
  check(test24(), 24, 2);
  check(test25(), 25, 97);
  check(test26(), 26, 37);
  check(test27(), 27, 8);
  check(test28(), 28, 67);
  check(test29(), 29, 12);
  check(test30(), 30, 7);
  check(test31(), 31, 7);
  check(test32(), 32, 2);
  check(test33(), 33, 7);
  check(test34(), 34, 5);
  check(test35(), 35, -6);
  check(test36(), 36, 16);
  check(test37(), 37, 4);
  check(test38(), 38, 11);
  check(test39(), 39, 28);
  check(test40(), 40, 2);
  check(test41(), 41, 12);
  check(test42(), 42, -2);
  check(test43(), 43, 1);
  check(test44(), 44, 36);
  check(test45(), 45, 11);
  check(test46(), 46, 56);

  if (failures == 0) {
    printf("\033[1;32mAll tests passed!\033[0m\n");
  } else {
    printf("\033[1;31m %d tests failed\033[0m\n", failures);
  }
  return failures;
}
