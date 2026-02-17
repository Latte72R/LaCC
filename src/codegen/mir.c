#include "mir.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <string.h>

// MirFunction に必要な命令数を確保する
static void ensure_inst_capacity(MirFunction *mf, int need) {
  if (mf->inst_cap >= need)
    return;

  int cap = mf->inst_cap ? mf->inst_cap : 16;
  while (cap < need)
    cap *= 2;

  MirInst *new_buf = realloc(mf->insts, sizeof(MirInst) * cap);
  if (!new_buf)
    error("memory allocation failed [in ensure_inst_capacity]");
  mf->insts = new_buf;
  mf->inst_cap = cap;
}

static const char *mir_op_name(MirOp op) {
  switch (op) {
  case MIR_OP_NOP:
    return "NOP";
  case MIR_OP_IMM:
    return "IMM";
  case MIR_OP_MOV:
    return "MOV";
  case MIR_OP_LOAD_LOCAL:
    return "LOAD_LOCAL";
  case MIR_OP_STORE_LOCAL:
    return "STORE_LOCAL";
  case MIR_OP_CAST:
    return "CAST";
  case MIR_OP_ADD:
    return "ADD";
  case MIR_OP_SUB:
    return "SUB";
  case MIR_OP_MUL:
    return "MUL";
  case MIR_OP_SDIV:
    return "SDIV";
  case MIR_OP_UDIV:
    return "UDIV";
  case MIR_OP_SMOD:
    return "SMOD";
  case MIR_OP_UMOD:
    return "UMOD";
  case MIR_OP_RET:
    return "RET";
  default:
    return "UNKNOWN";
  }
}

void mir_init(MirFunction *mf, Function *fn) {
  if (!mf)
    return;
  memset(mf, 0, sizeof(*mf));
  mf->fn = fn;
}

VReg mir_new_vreg(MirFunction *mf) {
  if (!mf)
    return MIR_INVALID_VREG;
  return mf->next_vreg++;
}

// MirFunction に命令を追加する
void mir_emit(MirFunction *mf, MirInst inst) {
  if (!mf)
    return;
  ensure_inst_capacity(mf, mf->inst_len + 1);
  mf->insts[mf->inst_len++] = inst;
}

// MirFunction の内容を人間が読める形式で出力する
void mir_dump(FILE *out, const MirFunction *mf) {
  if (!out || !mf)
    return;

  if (mf->fn) {
    fprintf(out, "MIR function %.*s (insts=%d, next_vreg=%d)\n", mf->fn->len, mf->fn->name, mf->inst_len,
            mf->next_vreg);
  } else {
    fprintf(out, "MIR function <null> (insts=%d, next_vreg=%d)\n", mf->inst_len, mf->next_vreg);
  }

  for (int i = 0; i < mf->inst_len; i++) {
    const MirInst *in = &mf->insts[i];
    fprintf(out, "  %4d: %-11s ", i, mir_op_name(in->op));
    switch (in->op) {
    case MIR_OP_IMM:
      fprintf(out, "v%d <- %ld", in->dst, in->imm);
      break;
    case MIR_OP_MOV:
      fprintf(out, "v%d <- v%d", in->dst, in->src1);
      break;
    case MIR_OP_LOAD_LOCAL:
      fprintf(out, "v%d <- local[%d]", in->dst, in->offset);
      break;
    case MIR_OP_STORE_LOCAL:
      fprintf(out, "local[%d] <- v%d", in->offset, in->src1);
      break;
    case MIR_OP_CAST:
      fprintf(out, "v%d <- cast(v%d)", in->dst, in->src1);
      break;
    case MIR_OP_ADD:
      fprintf(out, "v%d <- v%d + v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_SUB:
      fprintf(out, "v%d <- v%d - v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_MUL:
      fprintf(out, "v%d <- v%d * v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_SDIV:
      fprintf(out, "v%d <- v%d /s v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_UDIV:
      fprintf(out, "v%d <- v%d /u v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_SMOD:
      fprintf(out, "v%d <- v%d %%s v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_UMOD:
      fprintf(out, "v%d <- v%d %%u v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_RET:
      fprintf(out, "ret v%d", in->src1);
      break;
    case MIR_OP_NOP:
      fprintf(out, "nop");
      break;
    default:
      fprintf(out, "dst=%d src1=%d src2=%d imm=%ld off=%d", in->dst, in->src1, in->src2, in->imm, in->offset);
      break;
    }
    fprintf(out, "\n");
  }
}

void mir_free(MirFunction *mf) {
  if (!mf)
    return;
  free(mf->insts);
  mf->insts = NULL;
  mf->inst_len = 0;
  mf->inst_cap = 0;
  mf->next_vreg = 0;
  mf->fn = NULL;
}
