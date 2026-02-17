#ifndef MIR_H
#define MIR_H

#include "parser.h"

#include <stdio.h>

typedef int VReg;

enum {
  MIR_INVALID_VREG = -1,
};

typedef enum {
  MIR_OP_NOP,
  MIR_OP_IMM,
  MIR_OP_MOV,
  MIR_OP_LOAD_LOCAL,
  MIR_OP_STORE_LOCAL,
  MIR_OP_CAST,
  MIR_OP_ADD,
  MIR_OP_SUB,
  MIR_OP_MUL,
  MIR_OP_SDIV,
  MIR_OP_UDIV,
  MIR_OP_SMOD,
  MIR_OP_UMOD,
  MIR_OP_RET
} MirOp;

// 1命令分のデータ
typedef struct MirInst MirInst;
struct MirInst {
  MirOp op;
  VReg dst;
  VReg src1;
  VReg src2;
  long imm;
  int offset;
  Type *type;
};

// 関数1個分のMIR
typedef struct MirFunction MirFunction;
struct MirFunction {
  Function *fn;
  MirInst *insts;
  int inst_len;
  int inst_cap;
  int next_vreg;
};

void mir_init(MirFunction *mf, Function *fn);
VReg mir_new_vreg(MirFunction *mf);
void mir_emit(MirFunction *mf, MirInst inst);
void mir_dump(FILE *out, const MirFunction *mf);
void mir_free(MirFunction *mf);

#endif // MIR_H
