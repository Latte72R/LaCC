int sizeof_test1() {
  short a = 5;
  return sizeof(a) + a; /* 2 + 5 = 7 */
}

int sizeof_test2() {
  long a = 3;
  long b = 4;
  return sizeof(a) + a * b; /* 8 + 12 = 20 */
}

int sizeof_test3() {
  long long a = 1;
  long long b = 2;
  return sizeof(a) + a + b; /* 8 + 1 + 2 = 11 */
}

int sizeof_test4() {
  char a = 5;
  return sizeof(a + 1);
}

int sizeof_test5() {
  unsigned int a = 1;
  unsigned int b = -1;
  return a < b;
}

int sizeof_test6() {
  long long a = 1;
  char b = 2;
  return sizeof(a | b);
}

int sizeof_param_helper(int arr[10]) { return sizeof(arr); }

int sizeof_test7() {
  int arr[10];
  return sizeof_param_helper(arr);
}

int sizeof_test8() {
  // 後置インクリメントの返り値は元の値（unsigned char）
  unsigned char u = 255;
  int r = u++;
  return r; // 255
}
