// switch/goto/キャストまわり

/* goto の基本とラベルの組み合わせ（moved from literals_and_switch.inc） */
int switch_casts_test1() {
  int i = 0;
start:
  i++;
  if (i < 5) {
    goto start;
  }
  return i;
}

int switch_casts_test2() {
  int n = 0;
  goto ahead;
  n += 10;
ahead:
  n += 20;
  return n;
}

int switch_casts_test3() {
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

int switch_casts_test4() {
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

int switch_casts_test5() {
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

int switch_casts_test6() {
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

int switch_casts_test7() {
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

int switch_casts_test8() {
  /* 空のswitch文 */
  int x = 5;
  switch (x) {
    // 何もない
  }
  return 42; // switch後に実行される
}

int switch_casts_test9() {
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

int switch_casts_test10() {
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

int switch_casts_test11() {
  // Partial array initialization
  // Rest should be 0
  int arr[5] = {1, 2};
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}

// 単項演算子の組み合わせ
int switch_casts_test12() {
  int a = 5;
  return !!a + !0; // double negation + logical not of 0 = 1 + 1 = 2
}

// 配列の decay とポインタ演算
int test111_helper(int *arr) { return arr[0] + arr[1] + arr[2]; }

int switch_casts_test13() {
  /* Array decay to pointer */
  int arr[3] = {5, 7, 9};
  return test111_helper(arr); // 5 + 7 + 9 = 21
}

int switch_casts_test14() {
  char a = 5;
  return (int)a;
}

// ポインタを異なる型にキャストしてから戻す
int switch_casts_test15() {
  // Pointer to int cast (size dependent)
  int arr[5] = {10, 20, 30, 40, 50};
  int *ptr = &arr[2];
  // Cast pointer to different pointer type and back
  char *char_ptr = (char *)ptr;
  int *back_ptr = (int *)char_ptr;
  return *back_ptr; // Should return 30
}

// void ポインタのキャスト
int switch_casts_test16() {
  // Void pointer cast
  int value = 42;
  void *void_ptr = (void *)&value;
  int *int_ptr = (int *)void_ptr;
  return *int_ptr; // Should return 42
}

// 関数引数でのキャスト
int switch_casts_test17() {
  // Cast with function argument
  int arr[3] = {1, 2, 3};
  // Cast array to pointer explicitly
  int *ptr = (int *)arr;
  return ptr[1]; // Should return 2
}

// const 修飾子を外すキャスト
int switch_casts_test18() {
  const int arr[3] = {1, 2, 3};
  // Cast away constness (not recommended in practice)
  int *ptr = (int *)arr;
  *ptr = 10;                       // Modifying const data (undefined behavior)
  return arr[0] * arr[1] + arr[2]; // Should return 10 * 2 + 3 = 23
}

int switch_casts_test19() {
  int arr[5][7];
  return sizeof arr[2];
}

int switch_casts_test20() {
  int x = 0x7FFFFFFF;
  x += 0x7FFFFFFA;
  return x; // Should handle overflow correctly
}

// switch文内でのgoto
int switch_casts_test21() {
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
int switch_casts_test22() {
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
int switch_casts_test23() {
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
int switch_casts_test24() {
  /* keep as part of this group */
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

int switch_casts_test25() {
  // ネストした構造体のアクセス
  ST123 ns;
  ns.x = 5;
  ns.inner.a = 10;
  ns.inner.b[0] = 15;
  ns.inner.b[1] = 20;
  return ns.x + ns.inner.a + ns.inner.b[0] - ns.inner.b[1]; // 5 + 10 + 15 - 20 = 10
}

// 配列の部分初期化（残りは0で初期化される）
int switch_casts_test26() {
  int arr[5] = {1, 2};
  return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]; // 1 + 2 + 0 + 0 + 0 = 3
}

// 3次元配列のアクセスとsizeof
int switch_casts_test27() {
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

/* ポインタ/NULL/関数ポインタの代入・比較（moved from unsigned_and_suffixes.inc） */
int switch_casts_test28() { // 代入
  int *p = 0;
  int *p5 = (void *)0; // CではOK
  void *vp = 0;        // 当然OK
  char *cp = vp;       // CではOK（void* → T* 暗黙変換）

  // 関数ポインタ
  int f();
  int (*fp)() = 0;
  int (*fp2)() = (void *)0; // NULLが((void*)0)でもOKにできる

  // 比較
  if (p == 0) {
  }
  if (fp != 0) {
  }

  // 条件演算子
  int *q = (1 ? p : 0);
  int (*fq)() = (0 ? 0 : fp);
  return 0;
}
