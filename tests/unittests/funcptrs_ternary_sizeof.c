// 関数ポインタ(後半)、三項演算子、sizeof混合

/* test151/test152 は add/mul に依存するため、依存のないファイルへ移動しました */

int inc_test153(int x) { return x + 1; }

int fptr_ternary_sizeof_test1() {
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

int fptr_ternary_sizeof_test2() {
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

int fptr_ternary_sizeof_test3() {
  int a = 3;
  return a ? 7 : 5; /* 条件真 */
}

int fptr_ternary_sizeof_test4() {
  int a = 0;
  return a ? 7 : 5; /* 条件偽 */
}

int fptr_ternary_sizeof_test5() {
  int a = 1, b = 2, c = 3;
  return a ? (b ? 10 : 20) : (c ? 30 : 40); /* ネスト */
}

int fptr_ternary_sizeof_test6() {
  int x = 0;
  int y = (x++ ? x + 100 : x + 200); /* 偽側のみ評価 (x++ は 0 を返し x=1) */
  return y + x;                      /* 201 + 1 = 202 */
}

int fptr_ternary_sizeof_test7() {
  char c = 'A';
  int flag = 0;
  return flag ? c + 1 : c + 2; /* 'A'+2 = 67 */
}

int fptr_ternary_sizeof_test8() {
  int a = 2, b = 5;
  return (a < b ? a * b : a - b) + (b < a ? 1 : 2); /* 10 + 2 = 12 */
}

int fptr_ternary_sizeof_test9() {
  /* ポインタ選択 */
  int x = 5, y = 9;
  int *p = (x < y ? &x : &y);
  return *p; /* 5 */
}

int fptr_ternary_sizeof_test10() {
  /* ネスト + 副作用 (選ばれない側の c++ は評価されない) */
  int a = 1, b = 2, c = 3;
  int r = a ? (b++ ? b : 100) : (c++ ? 200 : 300);
  /* b++ は 2 を返し非ゼロ -> b オペランド採用。b は 3。c は未変更 */
  return r + b * 10 + c * 100; /* 3 + 30 + 300 = 333 */
}

int fptr_ternary_sizeof_test11() {
  /* 分岐内での代入と戻り値利用 */
  int a = 2, b = 3;
  int r = ((a += 1) > b ? (b *= 5) : (a *= 7)); /* a=3, 3>3 偽 -> a=21, r=21 */
  return a + b + r;                             /* 21 + 3 + 21 = 45 */
}

int fptr_ternary_sizeof_test12() {
  /* 右結合確認: x ? 1 : x+1 ? 2 : 3  == x ? 1 : (x+1 ? 2 : 3) */
  int x = 0;
  int r = x ? 1 : x + 1 ? 2 : 3; /* x=0 -> (0+1)真 -> 2 */
  return r;                      /* 2 */
}

int fptr_ternary_sizeof_test13() {
  /* || と 三項演算子の優先順位: a || b ? c : d == (a || b) ? c : d */
  int a = 0, b = 1;
  return a || b ? 5 : 9; /* (0||1)=真 -> 5 */
}

int fptr_ternary_sizeof_test14() {
  short a = 5;
  return sizeof(a) + a; /* 2 + 5 = 7 */
}

int fptr_ternary_sizeof_test15() {
  long a = 3;
  long b = 4;
  return sizeof(a) + a * b; /* 8 + 12 = 20 */
}

int fptr_ternary_sizeof_test16() {
  long long a = 1;
  long long b = 2;
  return sizeof(a) + a + b; /* 8 + 1 + 2 = 11 */
}

int fptr_ternary_sizeof_test17() {
  char a = 5;
  return sizeof(a + 1);
}

int fptr_ternary_sizeof_test18() {
  unsigned int a = 1;
  unsigned int b = -1;
  return a < b;
}

int fptr_ternary_sizeof_test19() {
  long long a = 1;
  char b = 2;
  return sizeof(a | b);
}

char test172_arr[][4] = {"abc", "def"};
int fptr_ternary_sizeof_test20() { return test172_arr[0][2] + test172_arr[1][1]; }

char *test173_arr[] = {"abc", "def"};
int fptr_ternary_sizeof_test21() { return test173_arr[0][0] + test173_arr[1][2]; }

int sizeof_param_helper(int arr[10]) { return sizeof(arr); }

int fptr_ternary_sizeof_test22() {
  int arr[10];
  return sizeof_param_helper(arr);
}

/* 三項演算子: ポインタと NULL の型が自然に選ばれる（moved from unsigned_and_suffixes.inc） */
int fptr_ternary_sizeof_test23() {
  int x = 42;
  int *p = &x;
  int *q = 0;
  int *r = (p ? p : (q ? q : 0));
  return (r == p); /* 1 */
}

// unsigned の基本動作テスト群
int fptr_ternary_sizeof_test24() {
  // 後置インクリメントの返り値は元の値（unsigned char）
  unsigned char u = 255;
  int r = u++;
  return r; // 255
}
