int cast_extra_test1() {
  int x = 0x1234;
  unsigned char c = (unsigned char)x;
  return (int)c;
}

int cast_extra_test2() {
  int x = 0x00F0;
  signed char c = (signed char)x;
  return (int)c;
}

int cast_extra_test3() {
  unsigned short us = 65535;
  return (int)us;
}

int cast_extra_test4() { return (_Bool)-123; }

int cast_extra_test5() {
  _Bool b = 2;
  return (int)b;
}

int cast_extra_test6() {
  unsigned int u = 0x80000000u;
  long long s = (long long)u;
  return s < 0;
}

int cast_extra_test7() {
  int x = 7;
  void *p = &x;
  int *q = (int *)p;
  return *q;
}

int cast_extra_test8() {
  long x = -1;
  unsigned int u = (unsigned int)x;
  return (int)((u >> 31) & 1u);
}

int cast_extra_test9() {
  long x = 0x12345678L;
  short s = (short)x;
  return (int)(unsigned short)s;
}

int cast_extra_test10() {
  long a = -1;
  unsigned int b = 0x80000000u;
  return a < (long)b;
}

int cast_extra_test11() {
  long a = -1;
  unsigned int b = 1u;
  return (unsigned long)a > (unsigned long)b;
}

int cast_extra_test12() {
  unsigned long x = 42UL;
  return (_Bool)x;
}
