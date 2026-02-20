// Regression tests for post-inc/dec value semantics.

int regress_postinc_test1() {
  int a = 3;
  int b = a++;
  return b * 10 + a; // 34
}

int regress_postinc_test2() {
  int a = 5;
  int b = a--;
  return b * 10 + a; // 54
}

int regress_postinc_test3() {
  int arr[3];
  int i = 0;
  arr[0] = 10;
  arr[1] = 20;
  arr[2] = 30;
  return arr[i++] + arr[i]; // 10 + 20 = 30
}

int regress_postinc_test4() {
  unsigned char u = 255;
  int r = u++;
  return r + u; // 255 + 0 = 255 (well-defined wrap for unsigned char)
}
