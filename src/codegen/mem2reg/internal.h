#ifndef CODEGEN_MEM2REG_INTERNAL_H
#define CODEGEN_MEM2REG_INTERNAL_H

#include "mir.h"

void run_mem2reg_prune_unreferenced_labels(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_mem2reg_fuse_compare_jz(MirInst **insts, int *inst_len);
void run_mem2reg_const_fold(MirInst **insts, int *inst_len, int next_vreg);
void run_mem2reg_canonicalize_commutative_imm_rhs(MirInst **insts, int *inst_len, int next_vreg);
void run_mem2reg_cfg_const_fold_branches(MirInst **insts, int *inst_len, int next_vreg);
void run_mem2reg_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg);
void run_mem2reg_dead_store_local_cfg(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_mem2reg_prune_unreachable_blocks(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void mem2reg_run_promote(MirFunction *mf);

#endif
