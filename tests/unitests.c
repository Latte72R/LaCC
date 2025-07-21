
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

int test47() {
  int a = 7, *b = &a, **c = &b;
  return **c;
}

int test48() {
  // 論理演算子の優先順位
  return 1 || 1 && 0;
}

int test49_helper(int x) {
  if (x % 2) {
    return x * 3;
  } else {
    return x + 3;
  }
}

int test49() {
  STRUCT arr[3];
  for (int i = 0; i < 3; i++) {
    arr[i].a = test49_helper(i + 1);
    arr[i].b[0] = i;
    arr[i].b[1] = i * 2;
  }
  int total = 0;
  for (int i = 0; i < 3; i++) {
    total += arr[i].a + (arr[i].b[0] + arr[i].b[1]);
  }
  return total;
}

int test50() {
  int v = 7;
  int *p = &v;
  int **pp = &p;
  int ***ppp = &pp;
  return ~***ppp + sizeof(**pp);
}

int test51() {
  int n = 10;
  int k = 0;
  while (--n) {
    k += n;
  }
  return k;
}

int test52() {
  int n = 10;
  int k = 0;
  k = n += 5;
  return k;
}

int test53() {
  int a = 0;
  do {
    a++;
  } while (a < 10);
  return a;
}

int test54() {
  int n = 5;
  return n * foo_test11(n = n - 3);
}

int test55() {
  int nn = 4;
  int nnn = 3;
  int n = 5;
  return n * nn * nnn;
}

int test56() {
  // 文字列リテラルの代入
  char str[15] = "Hello, "
                 "World!\n";
  return str[4] - str[1];
}

int test57() {
  // 文字列リテラルの代入
  int arr[3] = {3, 6, 2};
  return arr[0] + arr[1] * arr[2];
}

int test58() { return sizeof(int *); }

int test59() { return 'a' + 1; }

int test60() {
  /* ポインタの後置インクリメントと評価順 */
  int a[2] = {1, 2};
  int *p = a;
  return *p++ + *p; /* 1 + 2 = 3 */
}

int test61() {
  /* 2 段間接ポインタでの書き込み */
  int v = 6;
  int *p = &v;
  int **pp = &p;
  **pp += 4; /* v==10 */
  return v;
}

int test62() {
  /* sizeof 配列 vs sizeof ポインタ */
  int arr[10];
  int *p = arr;
  return sizeof(arr) / sizeof(*p); /* 10 */
}

int test63() {
  /* && の短絡評価で右辺が呼ばれないことを確認 */
  int a = 0, b = 0;
  if (a && (b = 1)) { /* 左辺が 0 なので右辺は評価されない */
    return -1;
  }
  return b; /* 0 なら OK */
}

int test64() {
  /* 後置 ++ と左辺値の取り扱い */
  int a = 2;
  return a++ * a; /* 2 * 3 = 6 */
}

int test65() {
  /* 括弧付きシフトと算術の優先順位 */
  return (2 + 1) << (1 + 1); /* 3 << 2 = 12 */
}

int test66() {
  /* char 配列でのポインタ差 */
  char buf[20];
  return &buf[15] - &buf[5]; /* 10 */
}

int test67() {
  /* 2 次元配列をポインタ算術で横断 */
  int m[3][4];
  m[2][3] = 7;
  return *(*(m + 2) + 3); /* 7 */
}

int test68() {
  /* || の短絡評価で副作用をスキップ */
  int a = 0;
  (1 || (a = 5)); /* 左辺が真なので右辺は評価されない */
  return a;       /* 0 */
}

void test69_sub(int arr[2][2]) {
  arr[0][0] = 5;
  arr[0][1] = 2;
  arr[1][0] = 3;
  arr[1][1] = 4;
}

int test69() {
  /* 2 次元配列を引数に渡す */
  int a[2][2];
  test69_sub(a);
  return a[0][0] * a[0][1] + a[1][0] * a[1][1];
}

int test70() {
  // hexadecimal literal
  return 0x5 * 0x3f; // 5 * 63 = 315
}

int test71() {
  // binary literal (GCC拡張)
  return 0b010110 | 0b001011 & 0b110011; // 0b010111 = 23
}

/* 複合代入演算子と式の値 */
int test72() {
  int a = 4, b = 5;
  return (a += 3) * (b -= 2); /* 7 * 3 = 21 */
}

/* int 配列でのポインタ差 (int 同士なので差は要素数) */
int test73() {
  int arr[10];
  return &arr[9] - &arr[4]; /* 5 */
}

/* 構造体の丸ごと代入が正しく動くか */
int test74() {
  STRUCT s1, s2;
  s1.a = 4;
  s1.b[0] = 2;
  s1.b[1] = 1;
  s2 = s1;                         /* 構造体のコピー */
  return s2.a + s2.b[0] + s2.b[1]; /* 4 + 2 + 1 = 7 */
}

/* sizeof でポインタ型を組み合わせた時の結果 */
int test75() { return sizeof(STRUCT *) + sizeof(char **); /* 8 + 8 = 16 (LP64 環境を想定) */ }

/* 文字定数の扱いと算術演算 */
int test76() { return '\n' + 1; /* 10 + 1 = 11 */ }

/* ビット演算とシフトの優先順位確認 */
int test77() { return 1 << 3 & 0b10110; }

int test78_sub(int n) {
  static int cnt = 0;
  cnt += n;
  return cnt;
}

int test78() { return test78_sub(3) * test78_sub(4) + test78_sub(2); }

int test79(void) {
  int i = 0;
start:
  i++;
  if (i < 5) {
    goto start;
  }
  return i;
}

int test80(void) {
  int n = 0;
  goto ahead;
  n += 10;
ahead:
  n += 20;
  return n;
}

int test81() {
  return sizeof('A'); // C標準ではint(4)
}

int test82() {
  /* 文字リテラルのエスケープシーケンス処理 */
  return '\a' + '\v'; // BEL(7) + VT(11) = 18
}

int test83() {
  // octal literal
  return 010 + 011; // 8 + 9 = 17
}

int test84() {
  /* 連続する文字列リテラルの連結 */
  char *s = "Hello"
            " "
            "World";
  return s[5]; /* ' ' = 32 - 文字列連結の実装 */
}

int test85() {
  /* 長い識別子名の処理 */
  int very_very_very_very_very_very_very_very_very_very_long_variable_name = 42;
  return very_very_very_very_very_very_very_very_very_very_long_variable_name;
}

int test86() {
  // 改行を含む文字列リテラル
  char *s = "line1\
line2";
  return s[6]; // '\' - 行継続の処理
}

int test87() {
  /* 演算子の優先順位の微妙なケース */
  return sizeof(int) + 1; /* sizeof演算子の優先順位 */
}

int test_id = 0;
void check(int result, int ans) {
  test_id++;
  if (result != ans) {
    printf("test%d failed (expected: %d / result: %d)\n", test_id, ans, result);
    failures++;
  }
}

int main() {
  failures = 0;
  check(test1(), 13);
  check(test2(), 2);
  check(test3(), 13);
  check(test4(), 1);
  check(test5(), 0);
  check(test6(), 1);
  check(test7(), 0);
  check(test8(), 10);
  check(test9(), 10);
  check(test10(), 2);
  check(test11(), 12);
  check(test12(), 3);
  check(test13(), 5);
  check(test14(), 25);
  check(test15(), 4);
  check(test16(), 28);
  check(test17(), 4);
  check(test18(), 34);
  check(test19(), 27);
  check(test20(), 7);
  check(test21(), 104);
  check(test22(), 12);
  check(test23(), 35);
  check(test24(), 2);
  check(test25(), 97);
  check(test26(), 37);
  check(test27(), 8);
  check(test28(), 67);
  check(test29(), 12);
  check(test30(), 7);
  check(test31(), 7);
  check(test32(), 2);
  check(test33(), 7);
  check(test34(), 5);
  check(test35(), -6);
  check(test36(), 16);
  check(test37(), 4);
  check(test38(), 11);
  check(test39(), 28);
  check(test40(), 2);
  check(test41(), 12);
  check(test42(), -2);
  check(test43(), 1);
  check(test44(), 36);
  check(test45(), 11);
  check(test46(), 56);
  check(test47(), 7);
  check(test48(), 1);
  check(test49(), 26);
  check(test50(), -4);
  check(test51(), 45);
  check(test52(), 15);
  check(test53(), 10);
  check(test54(), 40);
  check(test55(), 60);
  check(test56(), 10);
  check(test57(), 15);
  check(test58(), 8);
  check(test59(), 98);
  check(test60(), 3);
  check(test61(), 10);
  check(test62(), 10);
  check(test63(), 0);
  check(test64(), 6);
  check(test65(), 12);
  check(test66(), 10);
  check(test67(), 7);
  check(test68(), 0);
  check(test69(), 22);
  check(test70(), 315);
  check(test71(), 23);
  check(test72(), 21);
  check(test73(), 5);
  check(test74(), 7);
  check(test75(), 16);
  check(test76(), 11);
  check(test77(), 0);
  check(test78(), 30);
  check(test79(), 5);
  check(test80(), 20);
  check(test81(), 4);
  check(test82(), 18);
  check(test83(), 17);
  check(test84(), 32);
  check(test85(), 42);
  check(test86(), 105);
  check(test87(), 5);

  if (failures == 0) {
    printf("\033[1;36mAll %d tests passed!\033[0m\n", test_id);
  } else {
    printf("\033[1;31m %d of %d tests failed\033[0m\n", failures, test_id);
  }
  return failures;
}
