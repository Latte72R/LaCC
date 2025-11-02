// This is a comment.

void printf();
void *calloc();

int failures;

// 4倍にするヘルパー関数
int foo_test11(int n) { return n * 4; }

// ポインタ経由で値を5倍にするヘルパー関数
int foo_test14(int *n) {
  *n = *n * 5;
  return 0;
}

// 再帰によるフィボナッチ計算
int fibo_test18(int n) {
  if (n < 2)
    return n;
  else
    return fibo_test18(n - 1) + fibo_test18(n - 2);
}

// 四則演算と括弧を含む演算子優先順位のテスト
int test1() { return 5 * 3 - 12 / (2 + 4); }

// 単項マイナスと加減乗算の評価
int test2() { return -8 + 20 + 2 * (-5); }

// 剰余演算子のテスト
int test3() { return 5 + 35 % 9; }

// 等価比較 (==) のテスト
int test4() { return 1 == (3 - 2); }

// 関係演算子 (<) の結果 (偽=0) のテスト
int test5() {
  int a = 5;
  int b = 2;
  return a < b;
}

// 論理OR (||) と比較演算の組合せ
int test6() { return (1 == 2) || (8 > 4); }

// 論理AND (&&) と論理否定(!) の組合せ
int test7() { return (1 != 2) && !(8 > 4); }

// for ループとインクリメント
int test8() {
  int a = 5;
  int i;
  for (i = 0; i < 5; i++)
    a++;
  return a;
}

// while ループとインクリメント
int test9() {
  int a = 0;
  while (a < 10)
    a++;
  return a;
}

// if / else 分岐
int test10() {
  int a = 0;
  if (a < 1)
    return 2;
  else
    return 3;
}

// 単純な関数呼び出し
int test11() { return foo_test11(3); }

// アドレス演算子(&)と間接参照
int test12() {
  int a = 3;
  return *(&a);
}

// ポインタ経由の加算代入
int test13() {
  int a = 3;
  int *b = &a;
  *b += 2;
  return a;
}

// ポインタ引数経由での更新
int test14() {
  int a = 5;
  foo_test14(&a);
  return a;
}

int test15_sub;
// sizeof(グローバル変数) のテスト
int test15() { return sizeof(test15_sub); }

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

// 配列要素の更新と差分
int test17() {
  int a[2];
  a[0] = 3;
  a[1] = 5;
  a[0] *= 3;
  return a[0] - a[1];
}

// 再帰関数呼び出し (フィボナッチ)
int test18() { return fibo_test18(9); }

// for + continue の挙動
int test19() {
  int a = 0;
  for (int i = 0; i < 10; i++) {
    if (i % 3 == 0)
      continue;
    a += i;
  }
  return a;
}

// while + break の挙動
int test20() {
  int a = 0;
  while (a < 10) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

// 文字列リテラルと配列添字アクセス
int test21() {
  char *s = "hello";
  return s[0];
}

// char 配列と要素の積
int test22() {
  char str[2];
  str[0] = 3;
  str[1] = 4;
  return str[0] * str[1];
}

// calloc で確保した構造体とメンバ/配列アクセス
typedef struct {
  int a;
  int b[2];
} ST23;
int test23() {
  ST23 *c = calloc(1, sizeof(ST23));
  c->a = 5;
  c->b[0] = 1;
  c->b[1] = 6;
  return c->a * (c->b[0] + c->b[1]);
}

// enum の代入と返却
typedef enum { A24, B24, C24 } ENUM24;
ENUM24 test24() {
  ENUM24 a = C24;
  return a;
}

// 文字定数の返却
char test25() { return 'a'; }

// while + continue の挙動
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

// 後置インクリメントの評価順
int test27() {
  int a = 2;
  int b = a++;
  a++;
  return a * b;
}

// 単方向リスト状の next チェーンアクセス
typedef struct ST28 ST28;
struct ST28 {
  ST28 *next;
  char ch;
};
int test28() {
  ST28 *a = calloc(1, sizeof(ST28));
  ST28 *b = calloc(1, sizeof(ST28));
  ST28 *c = calloc(1, sizeof(ST28));
  c->ch = 'C';
  a->next = b;
  b->next = c;
  return a->next->next->ch;
}

// 前置インクリメントの評価
int test29() {
  int a = 2;
  int b = ++a;
  ++a;
  return a * b;
}

// 無限 for ループと break
int test30() {
  int a = 0;
  for (;;) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

// 代入式の値の評価
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

typedef struct {
  int a;
  int b[2];
} ST38;
int test38() {
  ST38 c;
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

typedef struct {
  int a;
  int b[2];
} ST41;
int test41() {
  // 構造体のサイズ
  // int(4) + int の合計 12 バイト
  return sizeof(ST41);
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

typedef struct {
  int a;
  int b[2];
} ST49;
int test49() {
  ST49 arr[3];
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

// 乗算と加算の組み合わせ
int test55() {
  int nn = 4;
  int nnn = 3;
  int n = 5;
  return n * nn * nnn;
}

// 文字列リテラルの代入
int test56() {
  char str[15] = "Hello, "
                 "World!\n";
  return str[4] - str[1];
}

// 配列の初期化と要素アクセス
int test57() {
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

// 段階的なポインタ操作
int test61() {
  int v = 6;
  int *p = &v;
  int **pp = &p;
  **pp += 4; /* v==10 */
  return v;
}

// 配列とポインタのサイズ比較
int test62() {
  int arr[10];
  int *p = arr;
  return sizeof(arr) / sizeof(*p); /* 10 */
}

// 短絡評価による副作用の確認
int test63() {
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

// 括弧付きシフトと算術の優先順位
int test65() { return (2 + 1) << (1 + 1); /* 3 << 2 = 12 */ }

// char 配列でのポインタ差
int test66() {
  char buf[20];
  return &buf[15] - &buf[5]; /* 10 */
}

// 2 次元配列をポインタ算術で横断
int test67() {
  int m[3][4];
  m[2][3] = 7;
  return *(*(m + 2) + 3); /* 7 */
}

// || の短絡評価で副作用をスキップ
int test68() {
  int a = 0;
  (1 || (a = 5)); /* 左辺が真なので右辺は評価されない */
  return a;       /* 0 */
}

// 2 次元配列を引数に渡す
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

// 16進数リテラルのテスト
int test70() {
  // hexadecimal literal
  return 0x5 * 0x3f; // 5 * 63 = 315
}

// 2進数リテラルのテスト (GCC拡張)
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
typedef struct {
  int a;
  int b[2];
} ST74;
int test74() {
  ST74 s1, s2;
  s1.a = 4;
  s1.b[0] = 2;
  s1.b[1] = 1;
  s2 = s1;                         /* 構造体のコピー */
  return s2.a + s2.b[0] + s2.b[1]; /* 4 + 2 + 1 = 7 */
}

/* sizeof でポインタ型を組み合わせた時の結果 */ typedef struct {
  int a;
  int b[2];
} ST75;
int test75() { return sizeof(ST75 *) + sizeof(char **); /* 8 + 8 = 16 (LP64 環境を想定) */ }

/* 文字定数の扱いと算術演算 */
int test76() { return '\n' + 1; /* 10 + 1 = 11 */ }

/* ビット演算とシフトの優先順位確認 */
int test77() { return 1 << 3 & 0b10110; }

int test78_helper(int n) {
  static int cnt = 0;
  cnt += n;
  return cnt;
}

// static 変数の副作用と演算
int test78() { return test78_helper(3) * test78_helper(4) + test78_helper(2); }

// goto の基本的な使い方
int test79(void) {
  int i = 0;
start:
  i++;
  if (i < 5) {
    goto start;
  }
  return i;
}

// ラベルと goto の組み合わせ
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

// 文字リテラルのエスケープシーケンス処理
int test82() {
  return '\a' + '\v'; // BEL(7) + VT(11) = 18
}

// octal literal
int test83() {
  return 010 + 011; // 8 + 9 = 17
}

// 連続する文字列リテラルの連結
int test84() {
  char *s = "Hello"
            " "
            "World";
  return s[5]; /* ' ' = 32 - 文字列連結の実装 */
}

// 長い識別子名の処理
int test85() {
  int very_very_very_very_very_very_very_very_very_very_long_variable_name = 42;
  return very_very_very_very_very_very_very_very_very_very_long_variable_name;
}

// 改行を含む文字列リテラル
int test86() {
  char *s = "line1\
line2";
  return s[6]; // '\' - 行継続の処理
}

// 演算子の優先順位の微妙なケース
int test87() { return sizeof(int) + 1; /* sizeof演算子の優先順位 */ }

// 複雑な式の評価
int test88() {
  int a = 1, b = 2, c = 3;
  int result = a + b * c << 1 & 7;
  return result; // 1 + 2 * 3 << 1 & 7 = 1 + 6 << 1 & 7 = 14 & 7 = 6
}

// ビット演算と比較演算の組み合わせ
int test89() {
  int result = 5 & 3 == 1;
  return result; // 5 & 3 == 1 は false (0) なので 0
}

// 基本的なswitch文
int test90() {
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

// fall-through動作のテスト
int test91() {
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

// default節のテスト
int test92() {
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

// ネストしたswitch文
int test93() {
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

// 文字定数を使ったswitch
int test94() {
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

// 複数のcaseラベルが同じ処理を共有
int test95() {
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

// switchの中でループとbreak/continueの混在
int test96() {
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

// switchの中でgoto
int test97() {
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

// 負の数のcase
int test98() {
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

// 16進数リテラルを使ったcase
int test99() {
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

typedef enum ENUM103 ENUM103;
enum ENUM103 { D103, E103, F103 };

int test103() {
  /* enumを使ったswitch */
  ENUM103 e = E103;
  switch (e) {
  case D103:
    return 0;
  case E103:
    return 1;
  case F103:
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

int test109() {
  // Partial array initialization
  // Rest should be 0
  int arr[5] = {1, 2};
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}

// 単項演算子の組み合わせ
int test110() {
  int a = 5;
  return !!a + !0; // double negation + logical not of 0 = 1 + 1 = 2
}

// 配列の decay とポインタ演算
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

// ポインタを異なる型にキャストしてから戻す
int test113() {
  // Pointer to int cast (size dependent)
  int arr[5] = {10, 20, 30, 40, 50};
  int *ptr = &arr[2];
  // Cast pointer to different pointer type and back
  char *char_ptr = (char *)ptr;
  int *back_ptr = (int *)char_ptr;
  return *back_ptr; // Should return 30
}

// void ポインタのキャスト
int test114() {
  // Void pointer cast
  int value = 42;
  void *void_ptr = (void *)&value;
  int *int_ptr = (int *)void_ptr;
  return *int_ptr; // Should return 42
}

// 関数引数でのキャスト
int test115() {
  // Cast with function argument
  int arr[3] = {1, 2, 3};
  // Cast array to pointer explicitly
  int *ptr = (int *)arr;
  return ptr[1]; // Should return 2
}

// const 修飾子を外すキャスト
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
  {
    r += 40;
  }
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

typedef struct {
  int a;
  int b[2];
} ST122;
int test122() {
  // 構造体ポインタから void* へのキャストとその逆
  ST122 s;
  s.a = 10;
  s.b[0] = 20;
  s.b[1] = 30;
  void *vp = (void *)&s;
  ST122 *sp = (ST122 *)vp;
  return sp->a + sp->b[0] - sp->b[1]; // 10 + 20 - 30 = 0
}

typedef struct {
  int x;
  ST122 inner;
} ST123;

int test123() {
  // ネストした構造体のアクセス
  ST123 ns;
  ns.x = 5;
  ns.inner.a = 10;
  ns.inner.b[0] = 15;
  ns.inner.b[1] = 20;
  return ns.x + ns.inner.a + ns.inner.b[0] - ns.inner.b[1]; // 5 + 10 + 15 - 20 = 10
}

// 配列の部分初期化（残りは0で初期化される）
int test124() {
  int arr[5] = {1, 2};
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}

// 3次元配列のアクセスとsizeof
int test125() {
  int arr3d[2][3][4];
  arr3d[1][2][3] = 42;

  // ポインタ経由でアクセス
  int *p = &arr3d[1][2][3];
  *p = 123;

  // 配列サイズの検証
  int size_row = sizeof(arr3d[0][0]);
  int row_count = sizeof(arr3d[0]) / size_row;

  return arr3d[1][2][3] + row_count;
}

int test126_sub[4];
int test126() {
  // グローバル配列のゼロ初期化
  return test126_sub[0] + test126_sub[1] + test126_sub[2] + test126_sub[3];
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

typedef struct {
  int a;
  int b[2];
} ST129;
int test129() {
  // 構造体ポインタの配列
  ST129 *arr[3];
  for (int i = 0; i < 3; i++) {
    arr[i] = (ST129 *)calloc(1, sizeof(ST129));
    arr[i]->a = i + 1;
    arr[i]->b[0] = i * 2;
    arr[i]->b[1] = i * 3;
  }
  return arr[0]->a + arr[1]->b[0] + arr[2]->b[1]; // 1 + 2 + 6 = 9
}

int test130() {
  // 複雑なビット演算の組み合わせ
  int a = 0xF0; // 11110000
  int b = 0xAA; // 10101010

  // シフトとAND、ORの組み合わせ
  int result = ((a & 0x0F) << 4) | (b & 0x0F);

  // ビット反転と優先順位
  result = ~(result & 0xFF) & 0xFF;

  return result; // 計算: ((0 << 4) | (10)) = 10、~10 & 0xFF = 0xF5 = 245
}

typedef struct ST131 ST131;
struct ST131 {
  int a;
  int b[2];
};
int test131() {
  // 構造体配列とポインタ操作
  ST131 structs[3];
  for (int i = 0; i < 3; i++) {
    structs[i].a = i + 1;
    structs[i].b[0] = i * 2;
    structs[i].b[1] = i * 3;
  }

  // ポインタ経由でアクセス
  ST131 *ps = structs;
  int *pi = (int *)ps;

  // 構造体メンバのオフセット計算（実装依存）
  pi[4] = 100; // これは structs[1].a を変更するはず

  return structs[0].a + structs[1].a + structs[2].a; // 1 + 100 + 3 = 104
}

int test132() {
  // static変数の初期値は 0
  static int static_var;
  return ++static_var;
}

typedef union {
  int i;
  int *ip;
  char c[4];
} UNION;

int test133() {
  // 基本的なunion使用
  UNION u;
  u.i = 45;
  return u.i; // 45
}

int test134() {
  // unionのサイズ確認（最大メンバのサイズ）
  return sizeof(UNION); // int(4)、 char*(8)、char[4](4) の中で最大の 8
}

int test135() {
  // ポインタunionのテスト
  UNION u;
  int value = 42;
  u.ip = &value;
  return *u.ip; // 42
}

int test136() {
  // unionメンバへのポインタアクセス
  UNION u;
  u.i = 0x61626364; // "abcd" in little endian
  char *cp = u.c;
  return cp[0] + cp[1]; // 'd' + 'c' = 100 + 99 = 199 (little endian前提)
}

int test137() {
  // unionの配列
  UNION arr[2];
  arr[0].i = 10;
  arr[1].i = 20;
  return arr[0].i + arr[1].i; // 30
}

int test138() {
  // unionポインタの間接参照
  UNION u;
  UNION *up = &u;
  up->i = 500;
  return up->i; // 500
}

typedef struct test139_struct test139_struct;
struct test139_struct {
  int a, b, c, d;
};
typedef union test139_union test139_union;
union test139_union {
  int arr[4];
  test139_struct s;
};
int test139() {
  test139_union u;
  u.arr[0] = 10;
  u.arr[1] = 20;
  u.arr[2] = 30;
  u.arr[3] = 40;
  return u.s.a + u.s.b + u.s.c + u.s.d; // 100
}

int test140() {
  // unionの全体代入
  UNION u1, u2;
  u1.i = 99;
  u2 = u1;
  return u2.i; // 99
}

int test141() {
  // void* ←→ 型付きポインタキャスト後のポインタ差分評価
  int arr[4];
  void *vp = &arr[3];
  return (int)((int *)vp - arr); // arr[3] - arr[0] = 3
}

/* 1) 基本的な別名 */
static int test142(void) {
  typedef int I;
  I x = 5;
  return x;
}

/* 2) ポインタ別名 */
static int test143(void) {
  typedef int *IP;
  int x = 0;
  IP p = &x;
  *p = 7;
  return x;
}

/* 3) sizeof(別名) の確認 */
static int test144(void) {
  typedef int I;
  return (int)sizeof(I); /* 想定: 4 */
}

/* 4) 無名 struct の typedef */
static int test145(void) {
  typedef struct {
    int w;
    char c;
  } Pair;
  Pair q;
  q.w = 4;
  q.c = 'A';
  return q.w + q.c; /* 4 + 65 = 69 */
}

/* 8) 連鎖 typedef */
static int test146(void) {
  typedef int I;
  typedef I J;
  typedef J K;
  K x = 5;
  return x; /* 5 */
}

int test147() {
  typedef int INT_ARR[3];
  INT_ARR arr = {5, 2, 3};
  return arr[0] * arr[1] + arr[2];
}

int test148() {
  int x = 1, y = 2, z = 3, w = 4, u = 5, v = 6;
  int *arr0[2];
  arr0[0] = &x;
  arr0[1] = &y;
  int *arr1[2];
  arr1[0] = &z;
  arr1[1] = &w;
  int *arr2[2];
  arr2[0] = &u;
  arr2[1] = &v;
  int *(*(a)[3])[2];
  a[0] = &arr0;
  a[1] = &arr1;
  a[2] = &arr2;
  int **tmp = *a[1];
  return *tmp[1];
}

static int *arr1_149[5];
static int *arr2_149[5];
int *(*select_arr_149(int which, int dummy))[5] {
  if (which)
    return &arr1_149;
  else
    return &arr2_149;
}
int test149() {
  int x = 3, y = 4, z = 5, w = 6, q = 7;
  arr1_149[0] = &x;
  arr1_149[1] = &y;
  arr1_149[2] = &z;
  arr1_149[3] = &w;
  arr1_149[4] = &q;
  int *(*(*a)(int, int))[5] = select_arr_149;
  int *(*r)[5] = select_arr_149(1, 2);
  return *(*r)[3];
}

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int (*factory(int which))(int, int) {
  if (which)
    return mul;
  else
    return add;
}
int test150() {
  int (*a)(int, int) = factory(0);
  int (*m)(int, int) = factory(1);
  return (*a)(3, 4) + m(5, 6);
}

int test151() {
  // 関数ポインタの配列
  int (*funcs[2])(int, int);
  funcs[0] = add;
  funcs[1] = mul;
  return funcs[0](2, 3) + funcs[1](4, 5); // 5 + 20 = 25
}

int test152() {
  // struct 内の関数ポインタ
  struct {
    int (*func)(int, int);
  } funcs[2];
  funcs[0].func = add;
  funcs[1].func = mul;
  return funcs[0].func(6, 7) + funcs[1].func(8, 9); // 13 + 72 = 85
}

int inc_test153(int x) { return x + 1; }

int test153() {
  // struct 内の配列ポインタと関数ポインタ
  struct {
    int (*func)(int);
    int (*arr)[2];
  } s;
  int a[2] = {3, 4};
  s.func = inc_test153;
  s.arr = &a;
  return s.func((*s.arr)[1]);
}

int test154() {
  // union 内の配列ポインタと関数ポインタ
  int a[2] = {7, 8};
  union {
    int (*func)(int), (*f)(int);
    int (*arr)[2];
  } u;
  u.func = inc_test153;
  int r = u.func(5);
  u.arr = &a;
  return r + (*u.arr)[0];
}

int test155() {
  int a = 3;
  return a ? 7 : 5; /* 条件真 */
}

int test156() {
  int a = 0;
  return a ? 7 : 5; /* 条件偽 */
}

int test157() {
  int a = 1, b = 2, c = 3;
  return a ? (b ? 10 : 20) : (c ? 30 : 40); /* ネスト */
}

int test158() {
  int x = 0;
  int y = (x++ ? x + 100 : x + 200); /* 偽側のみ評価 (x++ は 0 を返し x=1) */
  return y + x;                      /* 201 + 1 = 202 */
}

int test159() {
  char c = 'A';
  int flag = 0;
  return flag ? c + 1 : c + 2; /* 'A'+2 = 67 */
}

int test160() {
  int a = 2, b = 5;
  return (a < b ? a * b : a - b) + (b < a ? 1 : 2); /* 10 + 2 = 12 */
}

int test161() {
  /* ポインタ選択 */
  int x = 5, y = 9;
  int *p = (x < y ? &x : &y);
  return *p; /* 5 */
}

int test162() {
  /* ネスト + 副作用 (選ばれない側の c++ は評価されない) */
  int a = 1, b = 2, c = 3;
  int r = a ? (b++ ? b : 100) : (c++ ? 200 : 300);
  /* b++ は 2 を返し非ゼロ -> b オペランド採用。b は 3。c は未変更 */
  return r + b * 10 + c * 100; /* 3 + 30 + 300 = 333 */
}

int test163() {
  /* 分岐内での代入と戻り値利用 */
  int a = 2, b = 3;
  int r = ((a += 1) > b ? (b *= 5) : (a *= 7)); /* a=3, 3>3 偽 -> a=21, r=21 */
  return a + b + r;                             /* 21 + 3 + 21 = 45 */
}

int test164() {
  /* 右結合確認: x ? 1 : x+1 ? 2 : 3  == x ? 1 : (x+1 ? 2 : 3) */
  int x = 0;
  int r = x ? 1 : x + 1 ? 2 : 3; /* x=0 -> (0+1)真 -> 2 */
  return r;                      /* 2 */
}

int test165() {
  /* || と 三項演算子の優先順位: a || b ? c : d == (a || b) ? c : d */
  int a = 0, b = 1;
  return a || b ? 5 : 9; /* (0||1)=真 -> 5 */
}

int test166() {
  short a = 5;
  return sizeof(a) + a; /* 2 + 5 = 7 */
}

int test167() {
  long a = 3;
  long b = 4;
  return sizeof(a) + a * b; /* 8 + 12 = 20 */
}

int test168() {
  long long a = 1;
  long long b = 2;
  return sizeof(a) + a + b; /* 8 + 1 + 2 = 11 */
}

int test169() {
  char a = 5;
  return sizeof(a + 1);
}

int test170() {
  unsigned int a = 1;
  unsigned int b = -1;
  return a < b;
}

int test171() {
  long long a = 1;
  char b = 2;
  return sizeof(a | b);
}

char test172_arr[][4] = {"abc", "def"};
int test172() { return test172_arr[0][2] + test172_arr[1][1]; }

char *test173_arr[] = {"abc", "def"};
int test173() { return test173_arr[0][0] + test173_arr[1][2]; }

int sizeof_param_helper(int arr[10]) { return sizeof(arr); }

int test174() {
  int arr[10];
  return sizeof_param_helper(arr);
}

// unsigned の基本動作テスト群
int test175() {
  // 後置インクリメントの返り値は元の値（unsigned char）
  unsigned char u = 255;
  int r = u++;
  return r; // 255
}

int f_uc_ret_int(unsigned char x) { return x; }
int test176() {
  // 引数のゼロ拡張（unsigned char -> int）
  return f_uc_ret_int(255); // 255
}

int f_us_ret_int(unsigned short x) { return x; }
int test177() {
  // 引数のゼロ拡張（unsigned short -> int）
  return f_us_ret_int(65535); // 65535
}

int test178() {
  // 右シフトは unsigned では論理シフト
  unsigned int x = 0xF0000000;
  return x >> 28; // 15
}

// int test179() {
//   /* wraparound: 0xFFFFFFFFu + 2 -> 1 (mod 2^32) */
//   return 0xFFFFFFFFu + 2u;
// }

// int test180() {
//   /* underflow: 0u - 1u -> UINT_MAX -> (int) -1 (実装依存だが GCC/Clang/二の補数で -1) */
//   unsigned int x = 0u;
//   return (int)(x - 1u); /* -1 */
// }

// int test181() {
//   /* 符号付きvs非符号の比較: -1 < 1U ? 0 */
//   return (-1 < 1u);
// }

// int test182() {
//   /* 等値とキャスト: (unsigned)-1 は UINT_MAX */
//   return ((unsigned)-1 == 0xFFFFFFFFu);
// }

int test183() {
  /* 昇格: unsigned char は int に昇格して演算 */
  unsigned char uc = 250;
  return uc + 10; /* 260 */
}

int test184() {
  /* 昇格: unsigned short も int に昇格して演算 */
  unsigned short us = 65535;
  int r = us + 1; /* 65536 */
  return r;
}

// int test185() {
//   /* 右シフトは論理: 0x8000_0000 >> 31 == 1 */
//   unsigned int v = 0x80000000u;
//   return v >> 31;
// }

// int test186() {
//   /* 左シフトの定義動作(非符号): ビットが溢れても mod 2^32 */
//   return ((0xFFFFFFFFu << 1) == 0xFFFFFFFEu);
// }

// int test187() {
//   /* ~ とマスク: 非符号全1の下位8bitは 0xFF */
//   return (~0u) & 0xFFu; /* 255 */
// }

// int test188() {
//   /* 条件演算子の型決定: (int,-1) と (unsigned,1u) -> 共通型は unsigned */
//   int x = 1;
//   unsigned r = x ? -1 : 1u; /* -1 が unsigned に変換され UINT_MAX */
//   return (int)r;            /* -1 */
// }

// int test189() {
//   /* 負値を非符号へ, その後の剰余 */
//   return ((unsigned)-3) % 2u; /* 1 */
// }

int test190() {
  /* sizeof は size_t(非符号)。比較で -1 は size_t に変換され巨大値 */
  return (sizeof(int) > -1); /* 0 */
}

// int test191() {
//   /* 大きな非符号 / 3 の商 (4294967294 / 3) */
//   return ((unsigned)-2) / 3u; /* 1431655764 */
// }

int test192() {
  /* 符号混在比較その2: -2 < -1U ? 1 */
  return (-2 < (unsigned)-1);
}

// int test193() {
//   /* ビット演算の通常算術変換: -1 & 1U -> 1 */
//   return (-1 & 1u);
// }

int test194() {
  /* 整数昇格: (unsigned short)0 は int に昇格して ~0 -> -1 */
  return ~(unsigned short)0;
}

int test195() {
  /* 前置++のwrap: unsigned char 255 -> 0 */
  unsigned char u = 255;
  return ++u; /* 0 */
}

int test196() {
  /* 明示的アンダーフローの検出: 0u-1u == (unsigned)-1 */
  unsigned x = 0;
  x--;
  return x == (unsigned)-1; /* 1 */
}

// int test197() {
//   /* 符号混在の加算: 2U + (-1) == 1U */
//   return (2u + (-1)) == 1u; /* 1 */
// }

// int test198() {
//   /* wrap結果と比較: (0u-2) < 3u は偽 */
//   return ((0u - 2u) < 3u); /* 0 */
// }

int test199() {
  /* 型指定子の順序: long unsigned int が受理される */
  long unsigned int u = 7;
  return u; /* 7 */
}

int test200() {
  /* 三項演算子: ポインタと NULL の型が自然に選ばれる */
  int x = 42;
  int *p = &x;
  int *q = 0;
  int *r = (p ? p : (q ? q : 0));
  return (r == p); /* 1 */
}

int test201() {
  /* signed 指定子の受理と挙動（負値が保持される） */
  signed char a = -1;
  return a < 0; /* 1 */
}

int test202() {
  /* __builtin_va_list の typedef を受理（内部的に void* 相当として扱う） */
  typedef __builtin_va_list __my_va_list;
  __my_va_list ap;
  (void)ap;
  return 0; /* 単に構文が通ることを確認 */
}

int test203() {
  /* 配列次元内の定数式が受理・評価される */
  char _unused2[15 * sizeof(int) - 4 * sizeof(void *) - sizeof(unsigned long)];
  return sizeof(_unused2); /* LP64 なら 20 */
}

int test204() {
  /* 三項演算子の数値型混在（int と long long）を許容して評価 */
  long long v = (1 ? (long long)5 : 3);
  return (int)v; /* 5 */
}

int test205() { // 代入
  int *p = 0;
  int *p5 = (void *)0; // CではOK
  void *vp = 0;        // 当然OK
  char *cp = vp;       // CではOK（void* → T* 暗黙変換）

  // 関数ポインタ
  int f(void);
  int (*fp)(void) = 0;
  int (*fp2)(void) = (void *)0; // NULLが((void*)0)でもOKにできる

  // 比較
  if (p == 0) {
  }
  if (fp != 0) {
  }

  // 条件演算子
  int *q = (1 ? p : 0);
  int (*fq)(void) = (0 ? 0 : fp);
  return 0;
}

// 整数リテラル接尾辞のテスト群
int test206() {
  // sizeof の違い: 1(int)=4, 1L(long)=8, 1LL(long long)=8 (LP64)
  return sizeof(1) + sizeof(1L) + sizeof(1LL); // 4 + 8 + 8 = 20
}

int test207() {
  // unsigned リテラルにより右シフトが論理シフトになる
  return ((1u << 31) >> 31); // 1
}

int test208() {
  // U 接尾辞の sizeof は unsigned int だがサイズは int と同じ 4 (LP64)
  return sizeof(1U); // 4
}

int test209() {
  // 符号混在比較: -1 < 1u は偽 (0)
  return (-1 < 1u);
}

int test210() {
  // ULL 接尾辞: long long として扱うので 8 (LP64)
  return sizeof(1ULL); // 8
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
  check(test109(), 109, 3);
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
  check(test125(), 125, 126);
  check(test126(), 126, 0);
  check(test127(), 127, -4);
  check(test128(), 128, 100);
  check(test129(), 129, 9);
  check(test130(), 130, 245);
  check(test131(), 131, 6);
  check(test132(), 132, 1);
  check(test133(), 133, 45);
  check(test134(), 134, 8);
  check(test135(), 135, 42);
  check(test136(), 136, 199);
  check(test137(), 137, 30);
  check(test138(), 138, 500);
  check(test139(), 139, 100);
  check(test140(), 140, 99);
  check(test141(), 141, 3);
  check(test142(), 142, 5);
  check(test143(), 143, 7);
  check(test144(), 144, 4);
  check(test145(), 145, 69);
  check(test146(), 146, 5);
  check(test147(), 147, 13);
  check(test148(), 148, 4);
  check(test149(), 149, 6);
  check(test150(), 150, 37);
  check(test151(), 151, 25);
  check(test152(), 152, 85);
  check(test153(), 153, 5);
  check(test154(), 154, 13);
  check(test155(), 155, 7);
  check(test156(), 156, 5);
  check(test157(), 157, 10);
  check(test158(), 158, 202);
  check(test159(), 159, 67);
  check(test160(), 160, 12);
  check(test161(), 161, 5);
  check(test162(), 162, 333);
  check(test163(), 163, 45);
  check(test164(), 164, 2);
  check(test165(), 165, 5);
  check(test166(), 166, 7);
  check(test167(), 167, 20);
  check(test168(), 168, 11);
  check(test169(), 169, 4);
  check(test170(), 170, 1);
  check(test171(), 171, 8);
  check(test172(), 172, 200);
  check(test173(), 173, 199);
  check(test174(), 174, 8);
  check(test175(), 175, 255);
  check(test176(), 176, 255);
  check(test177(), 177, 65535);
  check(test178(), 178, 15);
  // check(test179(), 179, 1);
  // check(test180(), 180, -1);
  // check(test181(), 181, 0);
  // check(test182(), 182, 1);
  check(test183(), 183, 260);
  check(test184(), 184, 65536);
  // check(test185(), 185, 1);
  // check(test186(), 186, 1);
  // check(test187(), 187, 255);
  // check(test188(), 188, -1);
  // check(test189(), 189, 1);
  check(test190(), 190, 0);
  // check(test191(), 191, 1431655764);
  check(test192(), 192, 1);
  // check(test193(), 193, 1);
  check(test194(), 194, -1);
  check(test195(), 195, 0);
  check(test196(), 196, 1);
  // check(test197(), 197, 1);
  // check(test198(), 198, 0);
  check(test199(), 199, 7);
  check(test200(), 200, 1);
  check(test201(), 201, 1);
  check(test202(), 202, 0);
  check(test203(), 203, 20);
  check(test204(), 204, 5);
  check(test206(), 206, 20);
  check(test207(), 207, 1);
  check(test208(), 208, 4);
  check(test209(), 209, 0);
  check(test210(), 210, 8);

  if (failures == 0) {
    printf("\033[1;36mAll %d tests passed!\033[0m\n", test_cnt);
  } else {
    printf("\033[1;31m %d of %d tests failed\033[0m\n", failures, test_cnt);
  }
  return failures;
}
