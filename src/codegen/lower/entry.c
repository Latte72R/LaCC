#include "../codegen_internal.h"
#include "diagnostics.h"
#include "internal.h"
#include "platform.h"

#include <stdlib.h>

static int should_dump_mir() {
  const char *env = getenv("LACC_DUMP_MIR");
  if (!env || !env[0] || env[0] == '0')
    return 0;
  return 1;
}

void generate_assembly_pipeline(int optimize_level) {
  int level = optimize_level;
  if (level < 0)
    level = 0;
  int dump_mir = should_dump_mir();

  write_file(".intel_syntax noprefix\n");
#if !LACC_PLATFORM_APPLE
  write_file("  .section .note.GNU-stack,\"\",@progbits\n");
#endif
  gen_rodata_section();
  gen_data_section();
  emit_mir_program_pipeline(dump_mir, level);
}
