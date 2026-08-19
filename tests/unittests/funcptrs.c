int funcptrs_inc(int x) { return x + 1; }

int funcptrs_test1() {
  // struct 内の配列ポインタと関数ポインタ
  struct {
    int (*func)(int);
    int (*arr)[2];
  } s;
  int a[2] = {3, 4};
  s.func = funcptrs_inc;
  s.arr = &a;
  return s.func((*s.arr)[1]);
}

int funcptrs_test2() {
  // union 内の配列ポインタと関数ポインタ
  int a[2] = {7, 8};
  union {
    int (*func)(int), (*f)(int);
    int (*arr)[2];
  } u;
  u.func = funcptrs_inc;
  int r = u.func(5);
  u.arr = &a;
  return r + (*u.arr)[0];
}

char funcptrs_arr[][4] = {"abc", "def"};
int funcptrs_test3() { return funcptrs_arr[0][2] + funcptrs_arr[1][1]; }

char *funcptrs_ptr_arr[] = {"abc", "def"};
int funcptrs_test4() { return funcptrs_ptr_arr[0][0] + funcptrs_ptr_arr[1][2]; }
