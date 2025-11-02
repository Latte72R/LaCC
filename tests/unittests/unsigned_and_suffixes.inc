// 非符号の挙動・通常算術変換・sizeof・リテラル接尾辞

int unsigned_test1() {
  // 引数のゼロ拡張（unsigned char -> int）
  return f_uc_ret_int(255); // 255
}

int f_us_ret_int(unsigned short x) { return x; }
int unsigned_test2() {
  // 引数のゼロ拡張（unsigned short -> int）
  return f_us_ret_int(65535); // 65535
}

int unsigned_test3() {
  // 右シフトは unsigned では論理シフト
  unsigned int x = 0xF0000000;
  return x >> 28; // 15
}

int unsigned_test4() {
  /* wraparound: 0xFFFFFFFFu + 2 -> 1 (mod 2^32) */
  return 0xFFFFFFFFu + 2u;
}

int unsigned_test5() {
  /* underflow: 0u - 1u -> UINT_MAX -> (int) -1 (実装依存だが GCC/Clang/二の補数で -1) */
  unsigned int x = 0u;
  return (int)(x - 1u); /* -1 */
}

int unsigned_test6() {
  /* 符号付きvs非符号の比較: -1 < 1U ? 0 */
  return (-1 < 1u);
}

int unsigned_test7() {
  /* 等値とキャスト: (unsigned)-1 は UINT_MAX */
  return ((unsigned)-1 == 0xFFFFFFFFu);
}

int unsigned_test8() {
  /* 昇格: unsigned char は int に昇格して演算 */
  unsigned char uc = 250;
  return uc + 10; /* 260 */
}

int unsigned_test9() {
  /* 昇格: unsigned short も int に昇格して演算 */
  unsigned short us = 65535;
  int r = us + 1; /* 65536 */
  return r;
}

int unsigned_test10() {
  /* 右シフトは論理: 0x8000_0000 >> 31 == 1 */
  unsigned int v = 0x80000000u;
  return v >> 31;
}

int unsigned_test11() {
  /* 左シフトの定義動作(非符号): ビットが溢れても mod 2^32 */
  return ((0xFFFFFFFFu << 1) == 0xFFFFFFFEu);
}

int unsigned_test12() {
  /* ~ とマスク: 非符号全1の下位8bitは 0xFF */
  return (~0u) & 0xFFu; /* 255 */
}

int unsigned_test13() {
  /* 条件演算子の型決定: (int,-1) と (unsigned,1u) -> 共通型は unsigned */
  int x = 1;
  unsigned r = x ? -1 : 1u; /* -1 が unsigned に変換され UINT_MAX */
  return (int)r;            /* -1 */
}

int unsigned_test14() {
  /* 負値を非符号へ, その後の剰余 */
  return ((unsigned)-3) % 2u; /* 1 */
}

int unsigned_test15() {
  /* sizeof は size_t(非符号)。比較で -1 は size_t に変換され巨大値 */
  return (sizeof(int) > -1); /* 0 */
}

int unsigned_test16() {
  /* 大きな非符号 / 3 の商 (4294967294 / 3) */
  return ((unsigned)-2) / 3u; /* 1431655764 */
}

int unsigned_test17() {
  /* 符号混在比較その2: -2 < -1U ? 1 */
  return (-2 < (unsigned)-1);
}

int unsigned_test18() {
  /* ビット演算の通常算術変換: -1 & 1U -> 1 */
  return (-1 & 1u);
}

int unsigned_test19() {
  /* 整数昇格: (unsigned short)0 は int に昇格して ~0 -> -1 */
  return ~(unsigned short)0;
}

int unsigned_test20() {
  /* 前置++のwrap: unsigned char 255 -> 0 */
  unsigned char u = 255;
  return ++u; /* 0 */
}

int unsigned_test21() {
  /* 明示的アンダーフローの検出: 0u-1u == (unsigned)-1 */
  unsigned x = 0;
  x--;
  return x == (unsigned)-1; /* 1 */
}

int unsigned_test22() {
  /* 符号混在の加算: 2U + (-1) == 1U */
  return (2u + (-1)) == 1u; /* 1 */
}

int unsigned_test23() {
  /* wrap結果と比較: (0u-2) < 3u は偽 */
  return ((0u - 2u) < 3u); /* 0 */
}

int unsigned_test24() {
  /* 型指定子の順序: long unsigned int が受理される */
  long unsigned int u = 7;
  return u; /* 7 */
}

/* test200 は三項演算子の型決定に関するため funcptrs_ternary_sizeof.inc へ移動 */

int unsigned_test25() {
  /* signed 指定子の受理と挙動（負値が保持される） */
  signed char a = -1;
  return a < 0; /* 1 */
}

int unsigned_test26() {
  /* __builtin_va_list の typedef を受理（内部的に void* 相当として扱う） */
  typedef __builtin_va_list __my_va_list;
  __my_va_list ap;
  (void)ap;
  return 0; /* 単に構文が通ることを確認 */
}

int unsigned_test27() {
  /* 配列次元内の定数式が受理・評価される */
  char _unused2[15 * sizeof(int) - 4 * sizeof(void *) - sizeof(unsigned long)];
  return sizeof(_unused2); /* LP64 なら 20 */
}

int unsigned_test28() {
  /* 三項演算子の数値型混在（int と long long）を許容して評価 */
  long long v = (1 ? (long long)5 : 3);
  return (int)v; /* 5 */
}

/* test205 はポインタ/関数ポインタ代入に関するため switch_goto_casts.inc へ移動 */

// 整数リテラル接尾辞のテスト群
int unsigned_test29() {
  // sizeof の違い: 1(int)=4, 1L(long)=8, 1LL(long long)=8 (LP64)
  return sizeof(1) + sizeof(1L) + sizeof(1LL); // 4 + 8 + 8 = 20
}

int unsigned_test30() {
  // unsigned リテラルにより右シフトが論理シフトになる
  return ((1u << 31) >> 31); // 1
}

int unsigned_test31() {
  // U 接尾辞の sizeof は unsigned int だがサイズは int と同じ 4 (LP64)
  return sizeof(1U); // 4
}

int unsigned_test32() {
  // 符号混在比較: -1 < 1u は偽 (0)
  return (-1 < 1u);
}

int unsigned_test33() {
  // ULL 接尾辞: long long として扱うので 8 (LP64)
  return sizeof(1ULL); // 8
}
