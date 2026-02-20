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
  case MIR_OP_ADDR_LOCAL:
    return "ADDR_LOCAL";
  case MIR_OP_ADDR_SYMBOL:
    return "ADDR_SYMBOL";
  case MIR_OP_ADDR_FUNC:
    return "ADDR_FUNC";
  case MIR_OP_LOAD:
    return "LOAD";
  case MIR_OP_STORE:
    return "STORE";
  case MIR_OP_MEMCPY:
    return "MEMCPY";
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
  case MIR_OP_EQ:
    return "EQ";
  case MIR_OP_NE:
    return "NE";
  case MIR_OP_LT:
    return "LT";
  case MIR_OP_LE:
    return "LE";
  case MIR_OP_BITAND:
    return "BITAND";
  case MIR_OP_BITOR:
    return "BITOR";
  case MIR_OP_BITXOR:
    return "BITXOR";
  case MIR_OP_SHL:
    return "SHL";
  case MIR_OP_SHR:
    return "SHR";
  case MIR_OP_BITNOT:
    return "BITNOT";
  case MIR_OP_ADDR_STRING:
    return "ADDR_STRING";
  case MIR_OP_ADDR_ARRAY:
    return "ADDR_ARRAY";
  case MIR_OP_ADDR_STRUCT_LITERAL:
    return "ADDR_STRUCT";
  case MIR_OP_LABEL:
    return "LABEL";
  case MIR_OP_JMP:
    return "JMP";
  case MIR_OP_JZ:
    return "JZ";
  case MIR_OP_JCC:
    return "JCC";
  case MIR_OP_CALL:
    return "CALL";
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

int mir_new_label(MirFunction *mf) {
  if (!mf)
    return MIR_INVALID_LABEL;
  return mf->next_label++;
}

// MirFunction に命令を追加する
void mir_emit(MirFunction *mf, const MirInst *inst) {
  if (!mf || !inst)
    return;
  ensure_inst_capacity(mf, mf->inst_len + 1);
  mf->insts[mf->inst_len++] = *inst;
}

// MirFunction の内容を人間が読める形式で出力する
void mir_dump(FILE *out, const MirFunction *mf) {
  if (!out || !mf)
    return;

  if (mf->fn) {
    fprintf(out, "MIR function %.*s (insts=%d, next_vreg=%d, next_label=%d)\n", mf->fn->len, mf->fn->name,
            mf->inst_len, mf->next_vreg, mf->next_label);
  } else {
    fprintf(out, "MIR function <null> (insts=%d, next_vreg=%d, next_label=%d)\n", mf->inst_len, mf->next_vreg,
            mf->next_label);
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
    case MIR_OP_ADDR_LOCAL:
      fprintf(out, "v%d <- &local[%d]", in->dst, in->offset);
      break;
    case MIR_OP_ADDR_SYMBOL:
      if (in->var) {
        if (in->var->is_static && in->var->block > 0)
          fprintf(out, "v%d <- &%.*s.%d", in->dst, in->var->len, in->var->name, in->var->block);
        else
          fprintf(out, "v%d <- &%.*s", in->dst, in->var->len, in->var->name);
      } else {
        fprintf(out, "v%d <- &<null-symbol>", in->dst);
      }
      break;
    case MIR_OP_ADDR_FUNC:
      if (in->call_fn)
        fprintf(out, "v%d <- &%.*s", in->dst, in->call_fn->len, in->call_fn->name);
      else
        fprintf(out, "v%d <- &<null-function>", in->dst);
      break;
    case MIR_OP_LOAD:
      fprintf(out, "v%d <- *v%d", in->dst, in->src1);
      break;
    case MIR_OP_STORE:
      fprintf(out, "*v%d <- v%d", in->src1, in->src2);
      break;
    case MIR_OP_MEMCPY:
      fprintf(out, "memcpy(v%d <- v%d, size=%ld)", in->src1, in->src2, in->imm);
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
    case MIR_OP_EQ:
      fprintf(out, "v%d <- (v%d == v%d)", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_NE:
      fprintf(out, "v%d <- (v%d != v%d)", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_LT:
      fprintf(out, "v%d <- (v%d < v%d)", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_LE:
      fprintf(out, "v%d <- (v%d <= v%d)", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_BITAND:
      fprintf(out, "v%d <- v%d & v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_BITOR:
      fprintf(out, "v%d <- v%d | v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_BITXOR:
      fprintf(out, "v%d <- v%d ^ v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_SHL:
      fprintf(out, "v%d <- v%d << v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_SHR:
      fprintf(out, "v%d <- v%d >> v%d", in->dst, in->src1, in->src2);
      break;
    case MIR_OP_BITNOT:
      fprintf(out, "v%d <- ~v%d", in->dst, in->src1);
      break;
    case MIR_OP_ADDR_STRING:
      fprintf(out, "v%d <- &.L.str%d", in->dst, in->offset);
      break;
    case MIR_OP_ADDR_ARRAY:
      fprintf(out, "v%d <- &.L.arr%d", in->dst, in->offset);
      break;
    case MIR_OP_ADDR_STRUCT_LITERAL:
      fprintf(out, "v%d <- &.L.struct%d", in->dst, in->offset);
      break;
    case MIR_OP_LABEL:
      fprintf(out, "L%d:", in->label);
      break;
    case MIR_OP_JMP:
      fprintf(out, "jmp L%d", in->label);
      break;
    case MIR_OP_JZ:
      fprintf(out, "jz v%d, L%d", in->src1, in->label);
      break;
    case MIR_OP_JCC: {
      const char *cc = "??";
      if (in->imm == MIR_CC_EQ)
        cc = "eq";
      else if (in->imm == MIR_CC_NE)
        cc = "ne";
      else if (in->imm == MIR_CC_LT)
        cc = "lt";
      else if (in->imm == MIR_CC_LE)
        cc = "le";
      else if (in->imm == MIR_CC_GT)
        cc = "gt";
      else if (in->imm == MIR_CC_GE)
        cc = "ge";
      fprintf(out, "jcc.%s v%d, v%d, L%d", cc, in->src1, in->src2, in->label);
      break;
    }
    case MIR_OP_CALL:
      if (in->dst != MIR_INVALID_VREG)
        fprintf(out, "v%d <- ", in->dst);
      if (in->call_fn)
        fprintf(out, "call %.*s(", in->call_fn->len, in->call_fn->name);
      else
        fprintf(out, "call_indirect v%d(", in->src1);
      for (int a = 0; a < in->argc; a++) {
        if (a)
          fprintf(out, ", ");
        fprintf(out, "v%d", in->args[a]);
      }
      fprintf(out, ")");
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
  mf->next_label = 0;
  mf->fn = NULL;
}
