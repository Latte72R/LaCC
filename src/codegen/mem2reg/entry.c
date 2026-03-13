#include "../codegen_internal.h"
#include "internal.h"

void optimize_mir_mem2reg(MirFunction *mf) {
  if (!mf || mf->blocks[0].inst_len <= 0 || mf->next_vreg <= 0)
    return;
  mem2reg_run_promote(mf);
}
