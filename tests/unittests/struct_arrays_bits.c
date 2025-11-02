// 基本のインクリメント/配列/ビット演算/サイズ/ポインタ

int struct_bits_test1() {
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
int struct_bits_test2() {
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
int struct_bits_test3() {
  ST28 *a = calloc(1, sizeof(ST28));
  ST28 *b = calloc(1, sizeof(ST28));
  ST28 *c = calloc(1, sizeof(ST28));
  c->ch = 'C';
  a->next = b;
  b->next = c;
  return a->next->next->ch;
}

// 前置インクリメントの評価
int struct_bits_test4() {
  int a = 2;
  int b = ++a;
  ++a;
  return a * b;
}

// 無限 for ループと break
int struct_bits_test5() {
  int a = 0;
  for (;;) {
    if (a > 6)
      break;
    a++;
  }
  return a;
}

// 代入式の値の評価
int struct_bits_test6() {
  int a;
  return (a = 7);
}

int struct_bits_test7() {
  // ビットAND
  return 6 & 3; // 0b110 & 0b011 = 0b010 = 2
}

int struct_bits_test8() {
  // ビットOR
  return 6 | 3; // 0b110 | 0b011 = 0b111 = 7
}

int struct_bits_test9() {
  // ビットXOR
  return 6 ^ 3; // 0b110 ^ 0b011 = 0b101 = 5
}

int struct_bits_test10() {
  // ビットNOT
  return ~5; // ~0b0101 = ...11111010 = -6
}

int struct_bits_test11() {
  // 左シフト
  return 1 << 4; // 0b1 << 4 = 0b10000 = 16
}

int struct_bits_test12() {
  // 右シフト
  return 16 >> 2; // 0b10000 >> 2 = 0b100 = 4
}

typedef struct {
  int a;
  int b[2];
} ST38;
int struct_bits_test13() {
  ST38 c;
  c.a = 5;
  ((&c)->b)[0] = 3;
  c.b[1] = 4;
  return c.a * (&c)->b[0] - (&c)->b[1];
}

int struct_bits_test14() {
  // 演算子の優先順位
  return 1 + 2 * 3 << 2;
}

int struct_bits_test15() {
  // ポインタ差：配列要素間の差
  char arr[3];
  return &arr[2] - &arr[0]; // 2
}

typedef struct {
  int a;
  int b[2];
} ST41;
int struct_bits_test16() {
  // 構造体のサイズ
  // int(4) + int の合計 12 バイト
  return sizeof(ST41);
}

int struct_bits_test17() {
  // 演算子の優先順位
  return 1 + ~2;
}

int struct_bits_test18() {
  // 演算子の優先順位
  return 1 & 2 == 2;
}

int struct_bits_test19() {
  int n = 1;
  for (int i = 1; i < 4; i++) {
    n *= i;
  }
  for (int i = 1; i < 4; i++) {
    n *= i;
  }
  return n;
}

int struct_bits_test20() {
  int a = 2, b = 3, c = 5;
  return a * b + c;
}

int struct_bits_test21() {
  int arr[2][2];
  arr[0][0] = 2;
  arr[0][1] = 4;
  arr[1][0] = 6;
  arr[1][1] = 8;
  return arr[0][0] * arr[0][1] + arr[1][0] * arr[1][1];
}

int struct_bits_test22() {
  int a = 7, *b = &a, **c = &b;
  return **c;
}

int struct_bits_test23() {
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
int struct_bits_test24() {
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

int struct_bits_test25() {
  int v = 7;
  int *p = &v;
  int **pp = &p;
  int ***ppp = &pp;
  return ~***ppp + sizeof(**pp);
}
