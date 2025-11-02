// ループ/配列/ポインタ算術/文字列・数値リテラルなど

int loops_arrays_test1() {
  int n = 10;
  int k = 0;
  while (--n) {
    k += n;
  }
  return k;
}

int loops_arrays_test2() {
  int n = 10;
  int k = 0;
  k = n += 5;
  return k;
}

int loops_arrays_test3() {
  int a = 0;
  do {
    a++;
  } while (a < 10);
  return a;
}

// 乗算と加算の組み合わせ
int loops_arrays_test4() {
  int nn = 4;
  int nnn = 3;
  int n = 5;
  return n * nn * nnn;
}

// 文字列リテラルの代入
int loops_arrays_test5() {
  char str[15] = "Hello, "
                 "World!\n";
  return str[4] - str[1];
}

// 配列の初期化と要素アクセス
int loops_arrays_test6() {
  int arr[3] = {3, 6, 2};
  return arr[0] + arr[1] * arr[2];
}

int loops_arrays_test7() { return sizeof(int *); }

int loops_arrays_test8() { return 'a' + 1; }

// 段階的なポインタ操作
int loops_arrays_test9() {
  int v = 6;
  int *p = &v;
  int **pp = &p;
  **pp += 4; /* v==10 */
  return v;
}

// 配列とポインタのサイズ比較
int loops_arrays_test10() {
  int arr[10];
  int *p = arr;
  return sizeof(arr) / sizeof(*p); /* 10 */
}

// 短絡評価による副作用の確認
int loops_arrays_test11() {
  int a = 0, b = 0;
  if (a && (b = 1)) { /* 左辺が 0 なので右辺は評価されない */
    return -1;
  }
  return b; /* 0 なら OK */
}

// 括弧付きシフトと算術の優先順位
int loops_arrays_test12() { return (2 + 1) << (1 + 1); /* 3 << 2 = 12 */ }

// char 配列でのポインタ差
int loops_arrays_test13() {
  char buf[20];
  return &buf[15] - &buf[5]; /* 10 */
}

// 2 次元配列をポインタ算術で横断
int loops_arrays_test14() {
  int m[3][4];
  m[2][3] = 7;
  return *(*(m + 2) + 3); /* 7 */
}

// || の短絡評価で副作用をスキップ
int loops_arrays_test15() {
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

int loops_arrays_test16() {
  /* 2 次元配列を引数に渡す */
  int a[2][2];
  test69_helper(a);
  return a[0][0] * a[0][1] + a[1][0] * a[1][1];
}

// 16進数リテラルのテスト
int loops_arrays_test17() {
  // hexadecimal literal
  return 0x5 * 0x3f; // 5 * 63 = 315
}

// 2進数リテラルのテスト (GCC拡張)
int loops_arrays_test18() {
  // binary literal (GCC拡張)
  return 0b010110 | 0b001011 & 0b110011; // 0b010111 = 23
}

/* 複合代入演算子と式の値 */
int loops_arrays_test19() {
  int a = 4, b = 5;
  return (a += 3) * (b -= 2); /* 7 * 3 = 21 */
}

/* int 配列でのポインタ差 (int 同士なので差は要素数) */
int loops_arrays_test20() {
  int arr[10];
  return &arr[9] - &arr[4]; /* 5 */
}

/* 構造体の丸ごと代入が正しく動くか */
typedef struct {
  int a;
  int b[2];
} ST74;
int loops_arrays_test21() {
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
int loops_arrays_test22() { return sizeof(ST75 *) + sizeof(char **); /* 8 + 8 = 16 (LP64 環境を想定) */ }
