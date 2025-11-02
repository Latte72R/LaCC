// Tests for enum initializer with constant expressions

enum T1 { E0, E1 = 5, E2, E3 = 1 << 3, E4 };

int enum_init_test1() {
  int ok = 1;
  ok = ok && (E0 == 0);
  ok = ok && (E1 == 5);
  ok = ok && (E2 == 6);
  ok = ok && (E3 == 8);
  ok = ok && (E4 == 9);
  return ok;
}

// Ternary + relational inside enum initializer (mimic glibc _ISbit)
#define _ISbit(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
enum T2 { F0 = _ISbit(0), F8 = _ISbit(8) };
int enum_init_test2() {
  int ok = 1;
  ok = ok && (F0 == ((1 << 0) << 8));
  ok = ok && (F8 == ((1 << 8) >> 8));
  return ok;
}
