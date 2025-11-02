// 文字/文字列リテラル、switch 文バリエーション

int literals_switch_test1() { return '\n' + 1; /* 10 + 1 = 11 */ }

/* ビット演算とシフトの優先順位確認 */
int literals_switch_test2() { return 1 << 3 & 0b10110; }

int test78_helper(int n) {
  static int cnt = 0;
  cnt += n;
  return cnt;
}

// static 変数の副作用と演算
int literals_switch_test3() { return test78_helper(3) * test78_helper(4) + test78_helper(2); }

// goto の基本的な使い方
/* test79/test80 は goto に関するため switch_goto_casts.inc へ移動しました */

int literals_switch_test4() {
  return sizeof('A'); // C標準ではint(4)
}

// 文字リテラルのエスケープシーケンス処理
int literals_switch_test5() {
  return '\a' + '\v'; // BEL(7) + VT(11) = 18
}

// octal literal
int literals_switch_test6() {
  return 010 + 011; // 8 + 9 = 17
}

// 連続する文字列リテラルの連結
int literals_switch_test7() {
  char *s = "Hello"
            " "
            "World";
  return s[5]; /* ' ' = 32 - 文字列連結の実装 */
}

// 長い識別子名の処理
int literals_switch_test8() {
  int very_very_very_very_very_very_very_very_very_very_long_variable_name = 42;
  return very_very_very_very_very_very_very_very_very_very_long_variable_name;
}

// 改行を含む文字列リテラル
int literals_switch_test9() {
  char *s = "line1\
line2";
  return s[6]; // '\\' - 行継続の処理
}

// 演算子の優先順位の微妙なケース
int literals_switch_test10() { return sizeof(int) + 1; /* sizeof演算子の優先順位 */ }

// 複雑な式の評価
int literals_switch_test11() {
  int a = 1, b = 2, c = 3;
  int result = a + b * c << 1 & 7;
  return result; // 1 + 2 * 3 << 1 & 7 = 1 + 6 << 1 & 7 = 14 & 7 = 6
}

// ビット演算と比較演算の組み合わせ
int literals_switch_test12() {
  int result = 5 & 3 == 1;
  return result; // 5 & 3 == 1 は false (0) なので 0
}

// 基本的なswitch文
int literals_switch_test13() {
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
int literals_switch_test14() {
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
int literals_switch_test15() {
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
int literals_switch_test16() {
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
int literals_switch_test17() {
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
int literals_switch_test18() {
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
int literals_switch_test19() {
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
int literals_switch_test20() {
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
int literals_switch_test21() {
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
int literals_switch_test22() {
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

int literals_switch_test23() {
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
