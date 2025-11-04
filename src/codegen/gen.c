
#include "lacc.h"

extern void *NULL;

void generate_assembly() {
  // アセンブリのモード
  write_file(".intel_syntax noprefix\n");

  gen_rodata_section();
  gen_data_section();
  gen_text_section();
#if !LACC_PLATFORM_APPLE
  write_file("  .section .note.GNU-stack,\"\",@progbits\n");
#endif
}
