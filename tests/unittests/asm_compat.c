// Minimal inline assembly compatibility tests.

int asm_compat_test1(void) {
  unsigned int value = 0x01020304u;
  __asm__("" : "+r"(value));
  return (int)value;
}

int asm_compat_test2(void) {
  int value = 10;
  __asm__ volatile("" : "+r"(value));
  return value;
}

int asm_compat_test3(void) {
  int value = 21;
  __asm__("");
  return value;
}

int asm_compat_test4(void) {
  int value = 33;
  int input = 7;
  __asm__ volatile("" : "+r"(value) : "r"(input) : "cc", "memory");
  return value + input;
}
