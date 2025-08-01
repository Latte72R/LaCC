

int printf();
void *malloc();

// ttest.c からのテスト対象関数
int test(int x) { return x; }

// 1. int→pointer 変換警告
int test1() {
  int *a;
  // Warning: incompatible integer to pointer conversion
  return test(a);
}

// 5. ポインタ代入警告
int test5() {
  int x = 19;
  // Warning: incompatible integer to pointer conversion
  int *a = x;
  return 0;
}

// 6. 戻り値型の警告 (int 関数から pointer)
int test6() {
  char *str = "hello";
  // Warning: incompatible pointer to integer conversion
  int v = (int)str;
  return 0; // 期待値 0
}

// 7. ポインタを整数にキャストする警告
int test7() {
  char *ptr = "test";
  // Warning: incompatible pointer to integer conversion
  return (int)ptr;
}

// 8. ポインタを整数にキャストする警告
int *test8() {
  // Warning: incompatible pointer to integer conversion
  return 5;
}

int main() {
  test1();
  test5();
  test6();
  test7();
  test8();
}
