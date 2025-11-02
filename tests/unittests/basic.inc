// 基本演算・フロー・配列/文字/構造体の導入

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
int basic_test1() { return 5 * 3 - 12 / (2 + 4); }

// 単項マイナスと加減乗算の評価
int basic_test2() { return -8 + 20 + 2 * (-5); }

// 剰余演算子のテスト
int basic_test3() { return 5 + 35 % 9; }

// 等価比較 (==) のテスト
int basic_test4() { return 1 == (3 - 2); }

// 関係演算子 (<) の結果 (偽=0) のテスト
int basic_test5() {
  int a = 5;
  int b = 2;
  return a < b;
}

// 論理OR (||) と比較演算の組合せ
int basic_test6() { return (1 == 2) || (8 > 4); }

// 論理AND (&&) と論理否定(!) の組合せ
int basic_test7() { return (1 != 2) && !(8 > 4); }

// for ループとインクリメント
int basic_test8() {
  int a = 5;
  int i;
  for (i = 0; i < 5; i++)
    a++;
  return a;
}

// while ループとインクリメント
int basic_test9() {
  int a = 0;
  while (a < 10)
    a++;
  return a;
}

// if / else 分岐
int basic_test10() {
  int a = 0;
  if (a < 1)
    return 2;
  else
    return 3;
}

// 単純な関数呼び出し
int basic_test11() { return foo_test11(3); }

// アドレス演算子(&)と間接参照
int basic_test12() {
  int a = 3;
  return *(&a);
}

// ポインタ経由の加算代入
int basic_test13() {
  int a = 3;
  int *b = &a;
  *b += 2;
  return a;
}

// ポインタ引数経由での更新
int basic_test14() {
  int a = 5;
  foo_test14(&a);
  return a;
}

// sizeof(グローバル変数) のテスト
int basic_test15() { return sizeof(test15_sub); }

// 配列要素の更新と差分
int basic_test16() {
  int a[2];
  a[0] = 3;
  a[1] = 5;
  a[0] *= 3;
  return a[0] - a[1];
}

// 再帰関数呼び出し (フィボナッチ)
int basic_test17() { return fibo_test18(9); }

// for + continue の挙動
int basic_test18() {
  int a = 0;
  for (int i = 0; i < 10; i++) {
    if (i % 3 == 0)
      continue;
    a += i;
  }
  return a;
}

// while + break の挙動
int basic_test19() {
  int a = 0;
  while (a < 10) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

// 文字列リテラルと配列添字アクセス
int basic_test20() {
  char *s = "hello";
  return s[0];
}

// char 配列と要素の積
int basic_test21() {
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
int basic_test22() {
  ST23 *c = calloc(1, sizeof(ST23));
  c->a = 5;
  c->b[0] = 1;
  c->b[1] = 6;
  return c->a * (c->b[0] + c->b[1]);
}

// enum の代入と返却
typedef enum { A24, B24, C24 } ENUM24;
ENUM24 basic_test23() {
  ENUM24 a = C24;
  return a;
}

// 文字定数の返却
char basic_test24() { return 'a'; }
