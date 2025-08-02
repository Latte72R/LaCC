

int printf();
void *malloc();

// ttest.c からのテスト対象関数
int test(int x) { return x; }

// 1. int→pointer 変換警告
int test1() {
  int *a;
  return test(a);
}

// 2. 文字列初期化の配列宣言
void test2() { char str[5] = "Hello, World!"; }

// 3. 配列の初期化
void test3() { int arr[3] = {2, 4, 6, 5, 7}; }

// 4. const pointerの代入警告
void test4() {
  int y = 11;
  const int *x = &y;
  int *z;
  z = x;
}

// 5. ポインタ代入警告
void test5() {
  int x = 19;
  int *a = x;
}

// 6. 戻り値型の警告 (int 関数から pointer)
void test6() {
  char *str = "hello";
  int v = (int)str;
}

// 7. ポインタを整数にキャストする警告
int test7() {
  char *ptr = "test";
  return (int)ptr;
}

// 8. ポインタを整数にキャストする警告
int *test8() { return 5; }

int main() {
  test1();
  test5();
  test6();
  test7();
  test8();
}
