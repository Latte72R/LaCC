// Shared prelude (inline instead of separate include)

#include <stdio.h>

int failures;
int test_cnt = 0;
void check_with_name(int result, const char *name, int ans) {
  test_cnt++;
  if (result != ans) {
    printf("%s failed (expected: %d / result: %d)\n", name, ans, result);
    failures++;
  }
}

// Split test units (order matters for helper references)
#include "unittests/asm_compat.c"
#include "unittests/atomic_enum.c"
#include "unittests/basic.c"
#include "unittests/bool.c"
#include "unittests/builtins_fortify.c"
#include "unittests/comma.c"
#include "unittests/enum_init.c"
#include "unittests/funcptrs_ternary_sizeof.c"
#include "unittests/literals_and_switch.c"
#include "unittests/loops_strings_arrays.c"
#include "unittests/macro.c"
#include "unittests/nullable_funcptrs.c"
#include "unittests/struct_arrays_bits.c"
#include "unittests/struct_initializers.c"
#include "unittests/switch_goto_casts.c"
#include "unittests/unions_typedefs_funcptrs.c"
#include "unittests/unsigned_and_suffixes.c"

// Test invocation helpers: no need to pass numeric ids; use expression string instead
#define CHECK(EXPR, EXPECTED) check_with_name((EXPR), #EXPR, (EXPECTED))

int main() {
  failures = 0;
  CHECK(basic_test1(), 13);
  CHECK(basic_test2(), 2);
  CHECK(basic_test3(), 13);
  CHECK(basic_test4(), 1);
  CHECK(basic_test5(), 0);
  CHECK(basic_test6(), 1);
  CHECK(basic_test7(), 0);
  CHECK(basic_test8(), 10);
  CHECK(basic_test9(), 10);
  CHECK(basic_test10(), 2);
  CHECK(basic_test11(), 12);
  CHECK(basic_test12(), 3);
  CHECK(basic_test13(), 5);
  CHECK(basic_test14(), 25);
  CHECK(basic_test15(), 4);
  CHECK(basic_test16(), 4);
  CHECK(basic_test17(), 34);
  CHECK(basic_test18(), 27);
  CHECK(basic_test19(), 7);
  CHECK(basic_test20(), 104);
  CHECK(basic_test21(), 12);
  CHECK(basic_test22(), 35);
  CHECK(basic_test23(), 2);
  CHECK(basic_test24(), 97);
  CHECK(struct_bits_test1(), 37);
  CHECK(struct_bits_test2(), 8);
  CHECK(struct_bits_test3(), 67);
  CHECK(struct_bits_test4(), 12);
  CHECK(struct_bits_test5(), 7);
  CHECK(struct_bits_test6(), 7);
  CHECK(struct_bits_test7(), 2);
  CHECK(struct_bits_test8(), 7);
  CHECK(struct_bits_test9(), 5);
  CHECK(struct_bits_test10(), -6);
  CHECK(struct_bits_test11(), 16);
  CHECK(struct_bits_test12(), 4);
  CHECK(struct_bits_test13(), 11);
  CHECK(struct_bits_test14(), 28);
  CHECK(struct_bits_test15(), 2);
  CHECK(struct_bits_test16(), 12);
  CHECK(struct_bits_test17(), -2);
  CHECK(struct_bits_test18(), 1);
  CHECK(struct_bits_test19(), 36);
  CHECK(struct_bits_test20(), 11);
  CHECK(struct_bits_test21(), 56);
  CHECK(struct_bits_test22(), 7);
  CHECK(struct_bits_test23(), 1);
  CHECK(struct_bits_test24(), 26);
  CHECK(struct_bits_test25(), -4);
  CHECK(struct_init_test1(), 3);
  CHECK(struct_init_test2(), 94);
  CHECK(struct_init_test3(), 65);
  CHECK(struct_init_test4(), 18);

  CHECK(loops_arrays_test1(), 45);
  CHECK(loops_arrays_test2(), 15);
  CHECK(loops_arrays_test3(), 10);
  CHECK(loops_arrays_test4(), 60);
  CHECK(loops_arrays_test5(), 10);
  CHECK(loops_arrays_test6(), 15);
  CHECK(loops_arrays_test7(), 8);
  CHECK(loops_arrays_test8(), 98);
  CHECK(loops_arrays_test9(), 10);
  CHECK(loops_arrays_test10(), 10);
  CHECK(loops_arrays_test11(), 0);
  CHECK(loops_arrays_test12(), 12);
  CHECK(loops_arrays_test13(), 10);
  CHECK(loops_arrays_test14(), 7);
  CHECK(loops_arrays_test15(), 0);
  CHECK(loops_arrays_test16(), 22);
  CHECK(loops_arrays_test17(), 315);
  CHECK(loops_arrays_test18(), 23);
  CHECK(loops_arrays_test19(), 21);
  CHECK(loops_arrays_test20(), 5);
  CHECK(loops_arrays_test21(), 7);
  CHECK(loops_arrays_test22(), 16);

  CHECK(literals_switch_test1(), 11);
  CHECK(literals_switch_test2(), 0);
  CHECK(literals_switch_test3(), 30);
  CHECK(literals_switch_test4(), 4);
  CHECK(literals_switch_test5(), 18);
  CHECK(literals_switch_test6(), 17);
  CHECK(literals_switch_test7(), 32);
  CHECK(literals_switch_test8(), 42);
  CHECK(literals_switch_test9(), 105);
  CHECK(literals_switch_test10(), 5);
  CHECK(literals_switch_test11(), 6);
  CHECK(literals_switch_test12(), 0);
  CHECK(literals_switch_test13(), 20);
  CHECK(literals_switch_test14(), 30);
  CHECK(literals_switch_test15(), 42);
  CHECK(literals_switch_test16(), 12);
  CHECK(literals_switch_test17(), 2);
  CHECK(literals_switch_test18(), 2);
  CHECK(literals_switch_test19(), 42);
  CHECK(literals_switch_test20(), 25);
  CHECK(literals_switch_test21(), 20);
  CHECK(literals_switch_test22(), 255);
  CHECK(literals_switch_test23(), 40);

  CHECK(switch_casts_test1(), 5);
  CHECK(switch_casts_test2(), 20);
  CHECK(switch_casts_test3(), 99);
  CHECK(switch_casts_test4(), 3);
  CHECK(switch_casts_test5(), 1);
  CHECK(switch_casts_test6(), 20);
  CHECK(switch_casts_test7(), 20);
  CHECK(switch_casts_test8(), 42);
  CHECK(switch_casts_test9(), 0);
  CHECK(switch_casts_test10(), 3);
  CHECK(switch_casts_test11(), 3);
  CHECK(switch_casts_test12(), 2);
  CHECK(switch_casts_test13(), 21);
  CHECK(switch_casts_test14(), 5);
  CHECK(switch_casts_test15(), 30);
  CHECK(switch_casts_test16(), 42);
  CHECK(switch_casts_test17(), 2);
  CHECK(switch_casts_test18(), 23);
  CHECK(switch_casts_test19(), 28);
  CHECK(switch_casts_test20(), -7);
  CHECK(switch_casts_test21(), 230);
  CHECK(switch_casts_test22(), 150);
  CHECK(switch_casts_test23(), 70);
  CHECK(switch_casts_test24(), 0);
  CHECK(switch_casts_test25(), 10);
  CHECK(switch_casts_test26(), 3);

  CHECK(unions_funcptrs_test1(), 0);
  CHECK(unions_funcptrs_test2(), -4);
  CHECK(unions_funcptrs_test3(), 100);
  CHECK(unions_funcptrs_test4(), 9);
  CHECK(unions_funcptrs_test5(), 245);
  CHECK(unions_funcptrs_test6(), 6);
  CHECK(unions_funcptrs_test7(), 1);
  CHECK(unions_funcptrs_test8(), 45);
  CHECK(unions_funcptrs_test9(), 8);
  CHECK(unions_funcptrs_test10(), 42);
  CHECK(unions_funcptrs_test11(), 199);
  CHECK(unions_funcptrs_test12(), 30);
  CHECK(unions_funcptrs_test13(), 500);
  CHECK(unions_funcptrs_test14(), 100);
  CHECK(unions_funcptrs_test15(), 99);
  CHECK(unions_funcptrs_test16(), 3);
  CHECK(unions_funcptrs_test17(), 5);
  CHECK(unions_funcptrs_test18(), 7);
  CHECK(unions_funcptrs_test19(), 4);
  CHECK(unions_funcptrs_test20(), 69);
  CHECK(unions_funcptrs_test21(), 5);
  CHECK(unions_funcptrs_test22(), 13);
  CHECK(unions_funcptrs_test23(), 4);
  CHECK(unions_funcptrs_test24(), 6);
  CHECK(unions_funcptrs_test25(), 37);
  CHECK(nullable_fp_test1(), 6);
  CHECK(nullable_fp_test2(), 31);
  CHECK(nullable_fp_test3(), 11);
  CHECK(nullable_fp_test4(), 8);
  CHECK(atomic_enum_test1(), MO_RELEASE + MO_RELAXED + MO_SEQ_CST);
  CHECK(asm_compat_test1(), 0x01020304);
  CHECK(asm_compat_test2(), 10);
  CHECK(asm_compat_test3(), 21);
  CHECK(asm_compat_test4(), 40);

  CHECK(comma_basic(), 2);
  CHECK(comma_assignments(), 10);
  CHECK(comma_chained_side_effects(), 10);
  CHECK(comma_in_for_loop(), 18);

  CHECK(fptr_ternary_sizeof_test1(), 5);
  CHECK(fptr_ternary_sizeof_test2(), 13);
  CHECK(fptr_ternary_sizeof_test3(), 7);
  CHECK(fptr_ternary_sizeof_test4(), 5);
  CHECK(fptr_ternary_sizeof_test5(), 10);
  CHECK(fptr_ternary_sizeof_test6(), 202);
  CHECK(fptr_ternary_sizeof_test7(), 67);
  CHECK(fptr_ternary_sizeof_test8(), 12);
  CHECK(fptr_ternary_sizeof_test9(), 5);
  CHECK(fptr_ternary_sizeof_test10(), 333);
  CHECK(fptr_ternary_sizeof_test11(), 45);
  CHECK(fptr_ternary_sizeof_test12(), 2);
  CHECK(fptr_ternary_sizeof_test13(), 5);
  CHECK(fptr_ternary_sizeof_test14(), 7);
  CHECK(fptr_ternary_sizeof_test15(), 20);
  CHECK(fptr_ternary_sizeof_test16(), 11);
  CHECK(fptr_ternary_sizeof_test17(), 4);
  CHECK(fptr_ternary_sizeof_test18(), 1);
  CHECK(fptr_ternary_sizeof_test19(), 8);
  CHECK(fptr_ternary_sizeof_test20(), 200);
  CHECK(fptr_ternary_sizeof_test21(), 199);
  CHECK(fptr_ternary_sizeof_test22(), 8);
  CHECK(fptr_ternary_sizeof_test23(), 1);
  CHECK(fptr_ternary_sizeof_test24(), 255);

  CHECK(unsigned_test1(), 255);
  CHECK(unsigned_test2(), 65535);
  CHECK(unsigned_test3(), 15);
  CHECK(unsigned_test4(), 1);
  CHECK(unsigned_test5(), -1);
  CHECK(unsigned_test6(), 0);
  CHECK(unsigned_test7(), 1);
  CHECK(unsigned_test8(), 260);
  CHECK(unsigned_test9(), 65536);
  CHECK(unsigned_test10(), 1);
  CHECK(unsigned_test11(), 1);
  CHECK(unsigned_test12(), 255);
  CHECK(unsigned_test13(), -1);
  CHECK(unsigned_test14(), 1);
  CHECK(unsigned_test15(), 0);
  CHECK(unsigned_test16(), 1431655764);
  CHECK(unsigned_test17(), 1);
  CHECK(unsigned_test18(), 1);
  CHECK(unsigned_test19(), -1);
  CHECK(unsigned_test20(), 0);
  CHECK(unsigned_test21(), 1);
  CHECK(unsigned_test22(), 1);
  CHECK(unsigned_test23(), 0);
  CHECK(unsigned_test24(), 7);
  CHECK(unsigned_test25(), 1);
  CHECK(unsigned_test26(), 0);
  CHECK(unsigned_test27(), 20);
  CHECK(unsigned_test28(), 5);
  CHECK(unsigned_test29(), 20);
  CHECK(unsigned_test30(), 1);
  CHECK(unsigned_test31(), 4);
  CHECK(unsigned_test32(), 0);
  CHECK(unsigned_test33(), 8);
  CHECK(unsigned_test34(), 1);

  // bool / _Bool tests
  CHECK(bool_test1(), 2);
  CHECK(bool_test2(), 2);
  CHECK(bool_test3(), 2);
  CHECK(bool_test4(), 2);
  CHECK(bool_test5(), 1);
  CHECK(bool_test7(), 2);
  CHECK(bool_test8(), 2);
  CHECK(bool_test9(), 20);
  CHECK(bool_test10(), 3);
  CHECK(bool_test11(), 2);
  CHECK(bool_test12(), 1);
  CHECK(bool_test13(), 2);
  CHECK(bool_test14(), 7);

  // enum initializer tests
  CHECK(enum_init_test1(), 1);
  CHECK(enum_init_test2(), 1);

  CHECK(macro_test1(), 42);
  CHECK(macro_test2(), 25);
  CHECK(macro_test3(), 0);
  CHECK(macro_test4(), 21);
  CHECK(macro_test5(), 20);
  CHECK(macro_test6(), 123);
  CHECK(macro_test7(), 42);
  CHECK(macro_test8(), 1);
  CHECK(macro_test9(), 7);
  CHECK(macro_test10(), 1);
  CHECK(macro_test11(), 11);
  CHECK(macro_test12(), 12);
  CHECK(macro_test13(), 13);
  CHECK(macro_test14(), ANGLE_MAGIC);
  CHECK(macro_test15(), 15);
  CHECK(macro_test16(), 6);
  CHECK(macro_test18(), 1);
  CHECK(macro_test19(), 1);
  CHECK(macro_test20(), 1);
  CHECK(macro_test21(), 30);

  CHECK(builtin_object_size_test(), 1);
  CHECK(builtin_memcpy_chk_test(), 1);
  CHECK(builtin_memmove_chk_test(), 1);
  CHECK(builtin_memset_chk_test(), 1);
  CHECK(builtin_strcpy_chk_test(), 1);
  CHECK(builtin_strncpy_chk_test(), 1);
  CHECK(builtin_stpcpy_chk_test(), 1);

  if (failures == 0) {
    printf("\033[1;36mAll %d tests passed!\033[0m\n", test_cnt);
  } else {
    printf("\033[1;31m %d of %d tests failed\033[0m\n", failures, test_cnt);
  }
  return failures;
}
