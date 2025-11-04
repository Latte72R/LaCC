
#include "codegen.h"
#include "diagnostics.h"
#include "platform.h"

#include "codegen_internal.h"

extern void *NULL;

void generate_assembly() {
  // アセンブリのモード
  write_file(".intel_syntax noprefix\n");

#if !LACC_PLATFORM_APPLE
  write_file("  .section .note.GNU-stack,\"\",@progbits\n");
#endif

  gen_rodata_section();
  gen_data_section();
  gen_text_section();
}
