int ternary_test1() {
  int a = 3;
  return a ? 7 : 5; /* 条件真 */
}

int ternary_test2() {
  int a = 0;
  return a ? 7 : 5; /* 条件偽 */
}

int ternary_test3() {
  int a = 1, b = 2, c = 3;
  return a ? (b ? 10 : 20) : (c ? 30 : 40); /* ネスト */
}

int ternary_test4() {
  int x = 0;
  int y = (x++ ? x + 100 : x + 200); /* 偽側のみ評価 (x++ は 0 を返し x=1) */
  return y + x;                      /* 201 + 1 = 202 */
}

int ternary_test5() {
  char c = 'A';
  int flag = 0;
  return flag ? c + 1 : c + 2; /* 'A'+2 = 67 */
}

int ternary_test6() {
  int a = 2, b = 5;
  return (a < b ? a * b : a - b) + (b < a ? 1 : 2); /* 10 + 2 = 12 */
}

int ternary_test7() {
  /* ポインタ選択 */
  int x = 5, y = 9;
  int *p = (x < y ? &x : &y);
  return *p; /* 5 */
}

int ternary_test8() {
  /* ネスト + 副作用 (選ばれない側の c++ は評価されない) */
  int a = 1, b = 2, c = 3;
  int r = a ? (b++ ? b : 100) : (c++ ? 200 : 300);
  /* b++ は 2 を返し非ゼロ -> b オペランド採用。b は 3。c は未変更 */
  return r + b * 10 + c * 100; /* 3 + 30 + 300 = 333 */
}

int ternary_test9() {
  /* 分岐内での代入と戻り値利用 */
  int a = 2, b = 3;
  int r = ((a += 1) > b ? (b *= 5) : (a *= 7)); /* a=3, 3>3 偽 -> a=21, r=21 */
  return a + b + r;                             /* 21 + 3 + 21 = 45 */
}

int ternary_test10() {
  /* 右結合確認: x ? 1 : x+1 ? 2 : 3  == x ? 1 : (x+1 ? 2 : 3) */
  int x = 0;
  int r = x ? 1 : x + 1 ? 2 : 3; /* x=0 -> (0+1)真 -> 2 */
  return r;                      /* 2 */
}

int ternary_test11() {
  /* || と 三項演算子の優先順位: a || b ? c : d == (a || b) ? c : d */
  int a = 0, b = 1;
  return a || b ? 5 : 9; /* (0||1)=真 -> 5 */
}

/* 三項演算子: ポインタと NULL の型が自然に選ばれる */
int ternary_test12() {
  int x = 42;
  int *p = &x;
  int *q = 0;
  int *r = (p ? p : (q ? q : 0));
  return (r == p); /* 1 */
}
