#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "mir.h"

typedef enum {
  RA_LOC_NONE = 0,
  RA_LOC_REG,
  RA_LOC_STACK,
} RaLocKind;

typedef struct {
  RaLocKind kind;
  int reg;
  int stack_slot;
} RaVRegLoc;

typedef struct {
  int vreg_count;
  int spill_count;
  unsigned used_reg_mask;
  RaVRegLoc *locs;
} RegAllocResult;

enum {
  RA_PREG_COUNT = 8,
};

void gen_rodata_section();
void gen_data_section();

void optimize_mir_mem2reg(MirFunction *mf);
void optimize_mir_cleanup(MirFunction *mf);
void run_cleanup_prune_unreferenced_labels(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_cleanup_fuse_compare_jz(MirInst **insts, int *inst_len);
void run_cleanup_canonicalize_commutative_imm_rhs(MirInst **insts, int *inst_len, int next_vreg);
void run_cleanup_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg);
void run_cleanup_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg);
void run_cleanup_prune_unreachable_blocks(MirInst **insts, int *inst_len, int *inst_cap, int next_label);
void run_cleanup_sccp(MirFunction *mf);

const char *ra_preg64(int preg);
int ra_preg_is_callee_saved(int preg);
void regalloc_run(const MirFunction *mf, RegAllocResult *out);
void regalloc_free(RegAllocResult *out);

void emit_mir_function_internal(const MirFunction *mf);

void lower_function(Node *fn_node, MirFunction *mf);

#endif // CODEGEN_INTERNAL_H
