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
  MIR_OP_CALL,
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
  int label;
  LVar *var;
  Function *call_fn;
  int argc;
  VReg args[MAX_FUNC_PARAMS];
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
