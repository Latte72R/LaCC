#include "../codegen_internal.h"
#include "internal.h"

void optimize_mir_inline_cleanup(MirFunction *mf) {
  if (!mf || mf->inst_len <= 0 || mf->next_vreg <= 0)
    return;

  run_mem2reg_const_fold(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_canonicalize_commutative_imm_rhs(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_fuse_compare_jz(&mf->insts, &mf->inst_len);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_dead_store_local_cfg(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_cfg_const_fold_branches(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_canonicalize_commutative_imm_rhs(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_prune_unreachable_blocks(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_prune_unreferenced_labels(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
}

void optimize_mir_mem2reg(MirFunction *mf) {
  if (!mf || mf->inst_len <= 0 || mf->next_vreg <= 0)
    return;
  mem2reg_run_promote(mf);
}
