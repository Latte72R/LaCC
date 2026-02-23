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
  int a[3];
  int i = 0;
  a[0] = 5;
  a[1] = 6;
  a[2] = 7;
  int old = a[i++]++;
  return old * 100 + a[0] * 10 + i; // 561 (i is evaluated once)
}

int regress_postinc_test4() {
  unsigned char u = 255;
  int r = u++;
  return r + u; // 255 + 0 = 255 (well-defined wrap for unsigned char)
}
