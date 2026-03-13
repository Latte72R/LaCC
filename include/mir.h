#ifndef MIR_H
#define MIR_H

#include "parser.h"

#include <stdio.h>

typedef int VReg;

enum {
  MIR_INVALID_VREG = -1,
  MIR_INVALID_LABEL = -1,
};

typedef enum {
  MIR_OP_NOP,
  MIR_OP_IMM,
  MIR_OP_MOV,
  MIR_OP_LOAD_LOCAL,
  MIR_OP_STORE_LOCAL,
  MIR_OP_ADDR_LOCAL,
  MIR_OP_ADDR_SYMBOL,
  MIR_OP_ADDR_FUNC,
  MIR_OP_LOAD,
  MIR_OP_STORE,
  MIR_OP_MEMCPY,
  MIR_OP_CAST,
  MIR_OP_ADD,
  MIR_OP_SUB,
  MIR_OP_MUL,
  MIR_OP_SDIV,
  MIR_OP_UDIV,
  MIR_OP_SMOD,
  MIR_OP_UMOD,
  MIR_OP_EQ,
  MIR_OP_NE,
  MIR_OP_LT,
  MIR_OP_LE,
  MIR_OP_BITAND,
  MIR_OP_BITOR,
  MIR_OP_BITXOR,
  MIR_OP_SHL,
  MIR_OP_SHR,
  MIR_OP_BITNOT,
  MIR_OP_ADDR_STRING,
  MIR_OP_ADDR_ARRAY,
  MIR_OP_ADDR_STRUCT_LITERAL,
  MIR_OP_LABEL,
  MIR_OP_JMP,
  MIR_OP_JZ,
  MIR_OP_JCC,
  MIR_OP_CALL,
  MIR_OP_RET
} MirOp;

typedef enum {
  MIR_CC_EQ,
  MIR_CC_NE,
  MIR_CC_LT,
  MIR_CC_LE,
  MIR_CC_GT,
  MIR_CC_GE,
} MirCondCode;

// 1命令分のデータ
typedef struct MirInst MirInst;
struct MirInst {
  MirOp op;
  VReg dst;
  VReg src1;
  VReg src2;
  long imm;
  int offset;
  int label;
  LVar *var;
  Function *call_fn;
  int argc;
  VReg args[MAX_FUNC_PARAMS];
  Type *type;
};

typedef struct MirPhi MirPhi;
struct MirPhi {
  VReg dst;
  int *incoming_block;
  VReg *incoming_value;
  int incoming_count;
  int incoming_cap;
};

typedef struct MirBasicBlock MirBasicBlock;
struct MirBasicBlock {
  int id;
  int label;
  MirInst *insts;
  int inst_len;
  int inst_cap;
  int succ[2];
  int succ_count;
  int *preds;
  int pred_count;
  int pred_cap;
  MirPhi *phis;
  int phi_count;
  int phi_cap;
};

// 関数1個分のMIR
typedef struct MirFunction MirFunction;
struct MirFunction {
  Function *fn;
  MirBasicBlock *blocks;
  int block_count;
  int block_cap;
  int next_vreg;
  int next_label;
  int param_count;
  int param_offsets[MAX_FUNC_PARAMS];
  Type *param_types[MAX_FUNC_PARAMS];
};

void mir_init(MirFunction *mf, Function *fn);
VReg mir_new_vreg(MirFunction *mf);
int mir_new_label(MirFunction *mf);
void mir_emit(MirFunction *mf, const MirInst *inst);
void mir_dump(FILE *out, const MirFunction *mf);
void mir_free(MirFunction *mf);

#endif // MIR_H
