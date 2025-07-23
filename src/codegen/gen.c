
#include "lacc.h"

void generate_assembly() {
  // アセンブリのモード
  write_file(".intel_syntax noprefix\n");

  gen_rodata_section();
  gen_data_section();
  gen_text_section();
}
