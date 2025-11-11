// ゼロ初期化/ビット演算応用/構造体/union/typedef/関数ポインタ(前半)
int test126_sub[4];
int unions_funcptrs_test1() {
  // グローバル配列のゼロ初期化
  return test126_sub[0] + test126_sub[1] + test126_sub[2] + test126_sub[3];
}

int unions_funcptrs_test2() {
  // 符号付き整数の右シフト（実装依存の可能性あり）
  int negative = -16;
  return negative >> 2; // 算術右シフトなら -4、論理右シフトなら大きな正の値
}

int unions_funcptrs_test3() {
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
int unions_funcptrs_test4() {
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

int unions_funcptrs_test5() {
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
int unions_funcptrs_test6() {
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

int unions_funcptrs_test7() {
  // static変数の初期値は 0
  static int static_var;
  return ++static_var;
}

typedef union {
  int i;
  int *ip;
  char c[4];
} UNION;

int unions_funcptrs_test8() {
  // 基本的なunion使用
  UNION u;
  u.i = 45;
  return u.i; // 45
}

int unions_funcptrs_test9() {
  // unionのサイズ確認（最大メンバのサイズ）
  return sizeof(UNION); // int(4)、 char*(8)、char[4](4) の中で最大の 8
}

int unions_funcptrs_test10() {
  // ポインタunionのテスト
  UNION u;
  int value = 42;
  u.ip = &value;
  return *u.ip; // 42
}

int unions_funcptrs_test11() {
  // unionメンバへのポインタアクセス
  UNION u;
  u.i = 0x61626364; // "abcd" in little endian
  char *cp = u.c;
  return cp[0] + cp[1]; // 'd' + 'c' = 100 + 99 = 199 (little endian前提)
}

int unions_funcptrs_test12() {
  // unionの配列
  UNION arr[2];
  arr[0].i = 10;
  arr[1].i = 20;
  return arr[0].i + arr[1].i; // 30
}

int unions_funcptrs_test13() {
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
int unions_funcptrs_test14() {
  test139_union u;
  u.arr[0] = 10;
  u.arr[1] = 20;
  u.arr[2] = 30;
  u.arr[3] = 40;
  return u.s.a + u.s.b + u.s.c + u.s.d; // 100
}

int unions_funcptrs_test15() {
  // unionの全体代入
  UNION u1, u2;
  u1.i = 99;
  u2 = u1;
  return u2.i; // 99
}

int unions_funcptrs_test16() {
  // void* ←→ 型付きポインタキャスト後のポインタ差分評価
  int arr[4];
  void *vp = &arr[3];
  return (int)((int *)vp - arr); // arr[3] - arr[0] = 3
}

/* 1) 基本的な別名 */
static int unions_funcptrs_test17() {
  typedef int I;
  I x = 5;
  return x;
}

/* 2) ポインタ別名 */
static int unions_funcptrs_test18() {
  typedef int *IP;
  int x = 0;
  IP p = &x;
  *p = 7;
  return x;
}

/* 3) sizeof(別名) の確認 */
static int unions_funcptrs_test19() {
  typedef int I;
  return (int)sizeof(I); /* 想定: 4 */
}

/* 4) 無名 struct の typedef */
static int unions_funcptrs_test20() {
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
static int unions_funcptrs_test21() {
  typedef int I;
  typedef I J;
  typedef J K;
  K x = 5;
  return x; /* 5 */
}

int unions_funcptrs_test22() {
  typedef int INT_ARR[3];
  INT_ARR arr = {5, 2, 3};
  return arr[0] * arr[1] + arr[2];
}

int unions_funcptrs_test23() {
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
int unions_funcptrs_test24() {
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
int unions_funcptrs_test25() {
  int (*a)(int, int) = factory(0);
  int (*m)(int, int) = factory(1);
  return (*a)(3, 4) + m(5, 6);
}

/* 関数ポインタ(後半)の一部をこちらに集約（add/mul 依存を解消） */
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
