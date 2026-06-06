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

int regress_local_slot_test1(int x) {
  int a[3];
  long unused = x + 3;
  int b[3];
  a[0] = x;
  b[0] = x + 1;
  return a[0] + b[0];
}

int regress_ssa_if_test(int cond) {
  int value = 1;
  if (cond)
    value = 4;
  else
    value = 7;
  return value;
}

int regress_ssa_loop_test(int n) {
  int sum = 0;
  int i = 0;
  while (i < n) {
    sum = sum + i;
    i = i + 1;
  }
  return sum;
}
