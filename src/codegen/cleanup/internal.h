#ifndef CODEGEN_CLEANUP_INTERNAL_H
#define CODEGEN_CLEANUP_INTERNAL_H

#include "mir.h"

void run_cleanup_prune_unreferenced_labels(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_cleanup_fuse_compare_jz(MirInst **insts, int *inst_len);
void run_cleanup_canonicalize_commutative_imm_rhs(MirInst **insts, int *inst_len, int next_vreg);
void run_cleanup_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg);
void run_cleanup_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg);
void run_cleanup_prune_unreachable_blocks(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_cleanup_sccp(MirFunction *mf);

#endif
