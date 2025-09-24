
#include "lacc.h"

extern void *NULL;

void generate_assembly() {
  // アセンブリのモード
  write_file(".intel_syntax noprefix\n");
  write_file(".section .note.GNU-stack\n");

  gen_rodata_section();
  gen_data_section();
  gen_text_section();
}
