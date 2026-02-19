#ifndef CODEGEN_OPT_REGALLOC_H
#define CODEGEN_OPT_REGALLOC_H

#include "mir.h"

// vregの割り当て先
typedef enum {
  RA_LOC_NONE = 0,
  RA_LOC_REG,
  RA_LOC_STACK,
} RaLocKind;

// vregの割り当て先の詳細
typedef struct {
  RaLocKind kind;
  int reg;
  int stack_slot;
} RaVRegLoc;

// 関数全体のレジスタ割り当て結果
typedef struct {
  int vreg_count;
  int spill_count;
  unsigned used_reg_mask;
  RaVRegLoc *locs;
} RegAllocResult;

enum {
  RA_PREG_COUNT = 8,
};

const char *ra_preg64(int preg);
int ra_preg_is_callee_saved(int preg);
void regalloc_run(const MirFunction *mf, RegAllocResult *out);
void regalloc_free(RegAllocResult *out);

#endif // CODEGEN_OPT_REGALLOC_H
