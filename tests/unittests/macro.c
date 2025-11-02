/* Macro tests migrated from tests/macrotest.c
   Function names are prefixed with macro_ to avoid collisions with existing testN().
*/
#include "../angle_include.h"

int strcmp(const char *s1, const char *s2);

/* Macros under test */
#define CONST42 42
#define SQUARE(x) ((x) * (x))
#define STRIZE(x) #x
#define JOIN(a, b) a##b
#define MAKE_COUNTER(n) JOIN(counter_, n)
#define INC(x) ((x) + 1)
#define DOUBLE(x) ((x) * 2)
#define APPLY(fn, arg) fn(arg)
#define FLAG_TRUE 1
#define FLAG_FALSE 0
#define VAL 1 + 2
#define TO_REMOVE 13
#define TEST14_BASE 0
#define MULTILINE_COND 1
#define MULTI_LINE_MACRO(x) ((x) + 1)

int macro_test1() { return CONST42; }
int macro_test2() { return SQUARE(5); }
int macro_test3() {
  char *s = STRIZE(foo + bar);
  return strcmp(s, "foo + bar");
}

int macro_test4() {
  int counter_7 = 21;
  return MAKE_COUNTER(7);
}

int macro_test5() { return DOUBLE(APPLY(INC, 9)); }

#if FLAG_TRUE
int macro_test6() { return 123; }
#else
int macro_test6() { return -1; }
#endif

#if FLAG_FALSE
int macro_test7() { return -1; }
#elif defined(CONST42)
int macro_test7() { return CONST42; }
#else
int macro_test7() { return -2; }
#endif

#if VAL * 3 == 9
int macro_test8() { return 0; }
#else
int macro_test8() { return 1; }
#endif

int macro_test9() {
#if 1
#if 0
  return 0;
#else
  return 7;
#endif
#else
  return -1;
#endif
}

#if defined(UNDEFINED_MACRO)
int macro_test10() { return 0; }
#else
int macro_test10() { return 1; }
#endif

#ifdef CONST42
int macro_test11() { return 11; }
#else
int macro_test11() { return -11; }
#endif

#ifndef NOT_DEFINED_MACRO
int macro_test12() { return 12; }
#else
int macro_test12() { return -12; }
#endif

#ifdef TO_REMOVE
#undef TO_REMOVE
#endif

#ifdef TO_REMOVE
int macro_test13() { return -13; }
#else
int macro_test13() { return 13; }
#endif

int macro_test14() { return ANGLE_MACRO(TEST14_BASE); }

#if MULTILINE_COND && defined(CONST42)
int macro_test15() { return 15; }
#else
int macro_test15() { return -15; }
#endif

int macro_test16() { return MULTI_LINE_MACRO(5); }

#ifdef __LACC__
int macro_test17() { return __LACC__; }
#else
int macro_test17() { return -17; }
#endif

#ifdef __x86_64__
int macro_test18() { return __x86_64__; }
#else
int macro_test18() { return -18; }
#endif

#ifdef __LP64__
int macro_test19() { return __LP64__; }
#else
int macro_test19() { return -19; }
#endif

/* Block comment in macro */
#define WITH_BLOCK_COMMENT                                                                                             \
  0x20000000 /* this is a
                                          multi-line
                                          comment */
int macro_test20() { return WITH_BLOCK_COMMENT == 0x20000000; }

/* Cleanup: undef macros so they don't leak into other tests */
#undef CONST42
#undef SQUARE
#undef STRIZE
#undef JOIN
#undef MAKE_COUNTER
#undef INC
#undef DOUBLE
#undef APPLY
#undef FLAG_TRUE
#undef FLAG_FALSE
#undef VAL
#undef TO_REMOVE
#undef TEST14_BASE
#undef MULTILINE_COND
#undef MULTI_LINE_MACRO
#undef WITH_BLOCK_COMMENT
