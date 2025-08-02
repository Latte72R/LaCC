
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

/*
int test16() {
  // 未定義動作 (Incompatible pointer types)
  int a[2];
  *a = 7;
  *(a + 1) = 4;
  int *p = &a;
  return *p * *(p + 1);
}
*/

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

/*
int test54_helper(int n) { return n * 4; }
int test54() {
  // 未定義動作 (Unsequenced modification and access)
  // Clang と LaCC では 40, GCCでは 16
  int n = 5;
  return n * test54_helper(n = n - 3);
}
*/

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

/*
int test60() {
  // 未定義動作 (Unsequenced modification and access)
  int a[2] = {1, 2};
  int *p = a;
  return *p++ + *p;
}
*/

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

/*
int test64() {
  // 未定義動作
  int a = 2;
  return a++ * a;
  }
*/

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

void test69_helper(int arr[2][2]) {
  arr[0][0] = 5;
  arr[0][1] = 2;
  arr[1][0] = 3;
  arr[1][1] = 4;
}

int test69() {
  /* 2 次元配列を引数に渡す */
  int a[2][2];
  test69_helper(a);
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

int test78_helper(int n) {
  static int cnt = 0;
  cnt += n;
  return cnt;
}

int test78() { return test78_helper(3) * test78_helper(4) + test78_helper(2); }

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

int test88() {
  // 複雑な式の評価
  int a = 1, b = 2, c = 3;
  int result = a + b * c << 1 & 7;
  return result; // 1 + 2 * 3 << 1 & 7 = 1 + 6 << 1 & 7 = 14 & 7 = 6
}

int test89() {
  // ビット演算と比較演算の組み合わせ
  int result = 5 & 3 == 1;
  return result; // 5 & 3 == 1 は false (0) なので 0
}

int test90() {
  /* 基本的なswitch文 */
  int a = 2;
  int result = 0;
  switch (a) {
  case 1:
    result = 10;
    break;
  case 2:
    result = 20;
    break;
  case 3:
    result = 30;
    break;
  default:
    result = 99;
    break;
  }
  return result; // 20
}

int test91() {
  /* fall-through動作のテスト */
  int a = 1;
  int result = 0;
  switch (a) {
  case 1:
    result += 5;
  case 2:
    result += 10;
  case 3:
    result += 15;
    break;
  default:
    result = 99;
  }
  return result; // 5 + 10 + 15 = 30
}

int test92() {
  /* default節のテスト */
  int a = 99;
  switch (a) {
  case 1:
    return 10;
  case 2:
    return 20;
  default:
    return 42;
  }
  return 0; // 到達しない
}

int test93() {
  /* ネストしたswitch文 */
  int x = 1, y = 2;
  int result = 0;
  switch (x) {
  case 1:
    switch (y) {
    case 1:
      result = 11;
      break;
    case 2:
      result = 12;
      break;
    default:
      result = 19;
    }
    break;
  case 2:
    result = 20;
    break;
  default:
    result = 99;
  }
  return result; // 12
}

int test94() {
  /* 文字定数を使ったswitch */
  char c = 'B';
  int result = 0;
  switch (c) {
  case 'A':
    result = 1;
    break;
  case 'B':
    result = 2;
    break;
  case 'C':
    result = 3;
    break;
  default:
    result = 0;
  }
  return result; // 2
}

int test95() {
  /* 複数のcaseラベルが同じ処理を共有 */
  int day = 6; // 土曜日
  int result = 0;
  switch (day) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    result = 1; // 平日
    break;
  case 6:
  case 7:
    result = 2; // 週末
    break;
  default:
    result = 0; // 無効
  }
  return result; // 2
}

int test96() {
  /* switchの中でループとbreak/continueの混在 */
  int sum = 0;
  for (int i = 1; i <= 3; i++) {
    switch (i) {
    case 1:
      for (int j = 1; j <= 5; j++) {
        if (j == 3)
          continue;
        sum += j;
      }
      break;
    case 2:
      sum += 10;
      break;
    case 3:
      sum += 20;
      break;
    }
  }
  return sum; // (1+2+4+5) + 10 + 20 = 42
}

int test97() {
  /* switchの中でgoto */
  int result = 0;
  int n = 2;
  switch (n) {
  case 1:
    result = 10;
    goto end;
  case 2:
    result = 20;
    goto middle;
  case 3:
    result = 30;
    break;
  }
  result += 100; // case 3の時のみ実行される
middle:
  result += 5;
end:
  return result; // 25 (20 + 5)
}

int test98() {
  /* 負の数のcase */
  int x = -2;
  switch (x) {
  case -3:
    return 30;
  case -2:
    return 20;
  case -1:
    return 10;
  case 0:
    return 0;
  default:
    return 99;
  }
}

int test99() {
  /* 16進数リテラルを使ったcase */
  int x = 0xFF;
  switch (x) {
  case 0x00:
    return 0;
  case 0x10:
    return 16;
  case 0xFF:
    return 255;
  case 0x100:
    return 256;
  default:
    return -1;
  }
}

int test100_helper(int *counter) { return ++(*counter); }

int test100() {
  /* switchの式が副作用を持つ場合 */
  int counter = 1;

  switch (test100_helper(&counter)) {
  case 1:
    return counter * 10;
  case 2:
    return counter * 20; // counter = 40
  case 3:
    return counter * 30;
  default:
    return counter * 99;
  }
}

int test101() {
  /* default節がcaseの間にある場合 */
  int x = 5;
  int result = 0;
  switch (x) {
  case 1:
    result = 10;
    break;
  default:
    result = 99;
    break;
  case 2:
    result = 20;
    break;
  case 3:
    result = 30;
    break;
  }
  return result; // 99
}

int test102() {
  /* 同じ値のcaseラベル（コンパイルエラーになるべき）は避けて、
     代わりに複雑なfall-through */
  int x = 1;
  int result = 0;
  switch (x) {
  case 1:
    result += 1;
    /* fall through */
  case 2:
    result += 2;
    if (result > 2)
      break;
    /* fall through */
  case 3:
    result += 4;
    break;
  default:
    result = 99;
  }
  return result; // 3 (1 + 2, then break)
}

int test103() {
  /* enumを使ったswitch */
  ENUM e = B;
  switch (e) {
  case A:
    return 0;
  case B:
    return 1;
  case C:
    return 2;
  default:
    return -1;
  }
}

int test104() {
  // default節が最後にあるswitch
  int x = 2;
  int result = 0;
  switch (x) {
  default:
    result = 99;
    break;
  case 1:
    result = 10;
    break;
  case 2:
    result = 20;
    break;
  case 3:
    result = 30;
    break;
  }
  return result; // 20
}

int test105() {
  /* switch内で変数宣言とスコープ */
  int x = 2;
  switch (x) {
  case 1: {
    int local_var = 10;
    return local_var;
  }
  case 2: {
    int local_var = 20; // 同名だが別スコープ
    return local_var;
  }
  default:
    return 0;
  }
}

int test106() {
  /* 空のswitch文 */
  int x = 5;
  switch (x) {
    // 何もない
  }
  return 42; // switch後に実行される
}

int test107() {
  /* caseラベルだけでdefaultなし */
  int x = 99; // どのcaseにもマッチしない
  int result = 0;
  switch (x) {
  case 1:
    result = 10;
    break;
  case 2:
    result = 20;
    break;
  }
  return result; // 0 (何も実行されない)
}

int test108() {
  /* do-whileでbreak */
  int i = 0, sum = 0;
  do {
    if (i == 3)
      break;
    sum += i;
    i++;
  } while (i < 10);
  return sum; // 0 + 1 + 2 = 3
}

/*
int test109() {
  // Partial array initialization
  int arr[5] = {1, 2};                               // Rest should be 0
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}
*/

int test110() {
  /* Multiple unary operators */
  int a = 5;
  return !!a + !0; // double negation + logical not of 0 = 1 + 1 = 2
}

int test111_helper(int *arr) { return arr[0] + arr[1] + arr[2]; }

int test111() {
  /* Array decay to pointer */
  int arr[3] = {5, 7, 9};
  return test111_helper(arr); // 5 + 7 + 9 = 21
}

int test112() {
  char a = 5;
  return (int)a;
}

int test113() {
  // Pointer to int cast (size dependent)
  int arr[5] = {10, 20, 30, 40, 50};
  int *ptr = &arr[2];
  // Cast pointer to different pointer type and back
  char *char_ptr = (char *)ptr;
  int *back_ptr = (int *)char_ptr;
  return *back_ptr; // Should return 30
}

int test114() {
  // Void pointer cast
  int value = 42;
  void *void_ptr = (void *)&value;
  int *int_ptr = (int *)void_ptr;
  return *int_ptr; // Should return 42
}

int test115() {
  // Cast with function argument
  int arr[3] = {1, 2, 3};
  // Cast array to pointer explicitly
  int *ptr = (int *)arr;
  return ptr[1]; // Should return 2
}

int test116() {
  const int arr[3] = {1, 2, 3};
  // Cast away constness (not recommended in practice)
  int *ptr = (int *)arr;
  *ptr = 10;                       // Modifying const data (undefined behavior)
  return arr[0] * arr[1] + arr[2]; // Should return 10 * 2 + 3 = 23
}

int test117() {
  int arr[5][7];
  return sizeof arr[2];
}

int test118() {
  int x = 0x7FFFFFFF;
  x += 0x7FFFFFFA;
  return x; // Should handle overflow correctly
}

// switch文内でのgoto
int test119(void) {
  int r = 0, x = 2;
  switch (x) {
  case 2:
    goto c3;
  }
c3:
  r = 30; /* ← case をまたいで到達 */
  switch (x + 1) {
  case 3:
    r += 200;
  }
  return r; /* 30 + 200 */
}

// ネストしたブロック間でのgoto
int test120(void) {
  int r = 0;
  { /* 外側 */
    r += 10;
    goto in;
    {
    in:
      r += 20;
    } /* 内側ラベル */
    goto sib;
  }
sib:
  r += 30;
  { r += 40; }
  r += 50;
  return r; /* 10 + 20 + 30 + 40 + 50 */
}

// ラベルの前方参照・後方参照の混在
int test121(void) {
  int r = 0, i = 0;
start:
  r += 20;
  i++;
  if (i < 3)
    goto loop; /* 前方→後方 */
  goto end;
loop:
  r += 30;
  i++;
  if (i < 3)
    goto start; /* 後方→前方 */
end:
  return r; /* 20 + 30 + 20 */
}

int test122() {
  // 構造体ポインタから void* へのキャストとその逆
  STRUCT s;
  s.a = 10;
  s.b[0] = 20;
  s.b[1] = 30;
  void *vp = (void *)&s;
  STRUCT *sp = (STRUCT *)vp;
  return sp->a + sp->b[0] - sp->b[1]; // 10 + 20 - 30 = 0
}

typedef struct NESTED_STRUCT NESTED_STRUCT;
struct NESTED_STRUCT {
  int x;
  STRUCT inner;
};

int test123() {
  // ネストした構造体のアクセス
  NESTED_STRUCT ns;
  ns.x = 5;
  ns.inner.a = 10;
  ns.inner.b[0] = 15;
  ns.inner.b[1] = 20;
  return ns.x + ns.inner.a + ns.inner.b[0] - ns.inner.b[1]; // 5 + 10 + 15 - 20 = 10
}

int test124() {
  // 配列の部分初期化（残りは0で初期化される）
  int arr[5] = {1, 2};
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}

int test127() {
  // 符号付き整数の右シフト（実装依存の可能性あり）
  int negative = -16;
  return negative >> 2; // 算術右シフトなら -4、論理右シフトなら大きな正の値
}

int test128() {
  // const指定子の複雑な使用
  const int const_val = 42;
  const int *p1 = &const_val;
  int *p2 = (int *)p1; // const外し
  *p2 = 100;           // undefined behaviorだがコンパイルは通るはず
  int *const p3 = p2;  // ポインタ自体がconst
  return *p3;          // 100
}

int test129() {
  // 構造体ポインタの配列
  STRUCT *arr[3];
  for (int i = 0; i < 3; i++) {
    arr[i] = (STRUCT *)calloc(1, sizeof(STRUCT));
    arr[i]->a = i + 1;
    arr[i]->b[0] = i * 2;
    arr[i]->b[1] = i * 3;
  }
  return arr[0]->a + arr[1]->b[0] + arr[2]->b[1]; // 1 + 2 + 6 = 9
}

int test_cnt = 0;
void check(int result, int id, int ans) {
  test_cnt++;
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
  // check(test16(), 16, 28);
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
  check(test47(), 47, 7);
  check(test48(), 48, 1);
  check(test49(), 49, 26);
  check(test50(), 50, -4);
  check(test51(), 51, 45);
  check(test52(), 52, 15);
  check(test53(), 53, 10);
  // check(test54(), 54, 40);
  check(test55(), 55, 60);
  check(test56(), 56, 10);
  check(test57(), 57, 15);
  check(test58(), 58, 8);
  check(test59(), 59, 98);
  // check(test60(), 60, 3);
  check(test61(), 61, 10);
  check(test62(), 62, 10);
  check(test63(), 63, 0);
  // check(test64(), 64, 6);
  check(test65(), 65, 12);
  check(test66(), 66, 10);
  check(test67(), 67, 7);
  check(test68(), 68, 0);
  check(test69(), 69, 22);
  check(test70(), 70, 315);
  check(test71(), 71, 23);
  check(test72(), 72, 21);
  check(test73(), 73, 5);
  check(test74(), 74, 7);
  check(test75(), 75, 16);
  check(test76(), 76, 11);
  check(test77(), 77, 0);
  check(test78(), 78, 30);
  check(test79(), 79, 5);
  check(test80(), 80, 20);
  check(test81(), 81, 4);
  check(test82(), 82, 18);
  check(test83(), 83, 17);
  check(test84(), 84, 32);
  check(test85(), 85, 42);
  check(test86(), 86, 105);
  check(test87(), 87, 5);
  check(test88(), 88, 6);
  check(test89(), 89, 0);
  check(test90(), 90, 20);
  check(test91(), 91, 30);
  check(test92(), 92, 42);
  check(test93(), 93, 12);
  check(test94(), 94, 2);
  check(test95(), 95, 2);
  check(test96(), 96, 42);
  check(test97(), 97, 25);
  check(test98(), 98, 20);
  check(test99(), 99, 255);
  check(test100(), 100, 40);
  check(test101(), 101, 99);
  check(test102(), 102, 3);
  check(test103(), 103, 1);
  check(test104(), 104, 20);
  check(test105(), 105, 20);
  check(test106(), 106, 42);
  check(test107(), 107, 0);
  check(test108(), 108, 3);
  // check(test109(), 109, 3);
  check(test110(), 110, 2);
  check(test111(), 111, 21);
  check(test112(), 112, 5);
  check(test113(), 113, 30);
  check(test114(), 114, 42);
  check(test115(), 115, 2);
  check(test116(), 116, 23);
  check(test117(), 117, 28);
  check(test118(), 118, -7);
  check(test119(), 119, 230);
  check(test120(), 120, 150);
  check(test121(), 121, 70);
  check(test122(), 122, 0);
  check(test123(), 123, 10);
  check(test124(), 124, 3);
  check(test127(), 127, -4);
  check(test128(), 128, 100);
  check(test129(), 129, 9);

  if (failures == 0) {
    printf("\033[1;36mAll %d tests passed!\033[0m\n", test_cnt);
  } else {
    printf("\033[1;31m %d of %d tests failed\033[0m\n", failures, test_cnt);
  }
  return failures;
}
