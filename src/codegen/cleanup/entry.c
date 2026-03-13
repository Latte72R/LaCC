#include "../codegen_internal.h"
#include "internal.h"

void optimize_mir_cleanup(MirFunction *mf) {
  if (!mf || mf->blocks[0].inst_len <= 0 || mf->next_vreg <= 0)
    return;

  run_cleanup_fuse_compare_jz(&mf->blocks[0].insts, &mf->blocks[0].inst_len);
  run_cleanup_sccp(mf);
  run_cleanup_copyprop_and_dce(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_vreg);
  run_cleanup_canonicalize_commutative_imm_rhs(&mf->blocks[0].insts, &mf->blocks[0].inst_len, mf->next_vreg);
  run_cleanup_prune_unreachable_blocks(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
  run_cleanup_prune_unreferenced_labels(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
}
