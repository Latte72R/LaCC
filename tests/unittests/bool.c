// bool / _Bool の挙動テスト（C99）

#include <limits.h>
#include <stdbool.h>

int bool_test1() {
  // sizeof 確認: _Bool は 1 バイト、stdbool.h の bool も _Bool なので 1 バイト
  return (int)sizeof(_Bool) + (int)sizeof(bool); // 1 + 1 = 2
}

int bool_test2() {
  // true/false の基本確認（数値 1/0 で評価される）
  int ok_true = (true == 1);
  int ok_false = (false == 0);
  return ok_true + ok_false; // 2
}

int bool_test3() {
  // 整数からの変換: 0 -> 0, 非0 -> 1
  _Bool a = (bool)0;
  _Bool b = (bool)42;
  _Bool c = (bool)-5;
  return a + b + c; // 2
}

int bool_test4() {
  // _Bool への代入で正規化（0/1）される
  _Bool b = 0;
  b = 2; // -> 1
  int a = b;
  b = -1; // -> 1
  int c = b;
  return a + c; // 2
}

int bool_test5() {
  // ポインタ → ブール評価（NULL は 0、非NULL は 1）
  int x = 123;
  int *p = &x;
  int *q = 0;
  _Bool b1 = (p != 0); // 1
  _Bool b2 = (q != 0); // 0
  return b1 + b2;      // 1
}

int bool_test7() {
  // インクリメントの挙動: 0 -> 1、1 -> 2 だが格納時に 1 に丸められる
  _Bool b = 0;
  b++;            // 0 -> 1
  int r1 = b;     // 1
  b++;            // 1 -> 2 -> 格納で 1
  int r2 = b;     // 1
  return r1 + r2; // 2
}

int bool_test8() {
  // 論理演算子との組合せ
  bool b = true;
  int x = 0;
  int r1 = b && x;    // 0
  int r2 = x || b;    // 1
  return r1 + 2 * r2; // 2
}

int bool_test9() {
  // 条件演算子
  bool c = false;
  int r = c ? 10 : 20;
  return r; // 20
}

int bool_test10() {
  // 配列初期化時の正規化
  bool a[5] = {0, 1, 2, 0, -7};
  int sum = 0;
  for (int i = 0; i < 5; i++)
    sum += a[i]; // 0+1+1+0+1 = 3
  return sum;    // 3
}

int bool_test11() {
  // ビット単位演算（整数昇格後に演算）
  bool a = true, b = false;
  int r1 = a & b;              // 0
  int r2 = a | b;              // 1
  int r3 = a ^ a;              // 0
  return r1 + 2 * r2 + 4 * r3; // 2
}

int bool_test12() {
  // 比較演算の結果を _Bool に代入
  _Bool b1 = (3 < 5);  // 1
  _Bool b2 = (3 == 4); // 0
  return b1 + b2;      // 1
}

int bool_test13() {
  // 大きな整数（符号なし/符号あり）→ _Bool
  _Bool a = (_Bool)UINT_MAX; // 1
  _Bool b = (_Bool)(-123);   // 1
  _Bool c = (_Bool)0U;       // 0
  return a + b + c;          // 2
}

int bool_test14() {
  // 三項演算子での bool 値選択
  bool b = true;
  int r = b ? 7 : 9;
  return r; // 7
}
