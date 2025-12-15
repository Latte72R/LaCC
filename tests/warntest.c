
// 2. 配列初期化の警告
void test2() { char str[5] = "Hello, World!"; } // 初期化子過多

// 3. 配列要素数の警告
void test3() { int arr[3] = {2, 4, 6, 5, 7}; } // 要素過多

// 4. const修飾子の無視
void test4() {
  int y = 11;
  const int *x = &y;
  int *z;
  z = x;
}

// 5. 整数→ポインタの変換
void test5() {
  int y = 11;
  int *x = &y;
  char *z;
  z = x;
}

// 6. ポインタ→整数の変換
void test6() {
  char *str = "hello";
  int v = (int)str;
  (void)v;
}

// 14. ポインタと整数の比較
void test14() {
  char *p = 0;
  int i = 0;
  if (p == i) {
    i++;
  }
}

// 23. enum の異種比較
enum E1 { EA, EB };
enum E2 { EC, ED };
void test23() {
  enum E1 a = EA;
  enum E2 b = EC;
  int x = a == b; // 別enum同士
}

#warning This is a test warning from preprocessor. \
The lines below will be compiled.

int main() {
  test2();
  test3();
  test4();
  test5();
  test6();
  test14();
  test23();

  return 0;
}
