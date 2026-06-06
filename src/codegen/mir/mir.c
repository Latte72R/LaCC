#include "diagnostics.h"
#include "mir.h"

#include <stdlib.h>
#include <string.h>

static void ensure_bb_inst_capacity(MirBasicBlock *bb, int need) {
  if (bb->inst_cap >= need)
    return;
  int cap = bb->inst_cap ? bb->inst_cap : 16;
  while (cap < need)
    cap *= 2;
  MirInst *new_buf = realloc(bb->insts, sizeof(MirInst) * cap);
  if (!new_buf)
    error("memory allocation failed [in ensure_bb_inst_capacity]");
  bb->insts = new_buf;
  bb->inst_cap = cap;
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

int mir_add_local_slot(MirFunction *mf, LVar *var, Type *type, int size) {
  if (!mf || !type || size <= 0)
    error("invalid local slot [in mir_add_local_slot]");
  if (mf->local_count >= mf->local_cap) {
    int cap = mf->local_cap ? mf->local_cap * 2 : 16;
    LVar **new_vars = realloc(mf->local_vars, sizeof(LVar *) * cap);
    Type **new_types = realloc(mf->local_types, sizeof(Type *) * cap);
    int *new_sizes = realloc(mf->local_sizes, sizeof(int) * cap);
    if (!new_vars || !new_types || !new_sizes)
      error("memory allocation failed [in mir_add_local_slot]");
    mf->local_vars = new_vars;
    mf->local_types = new_types;
    mf->local_sizes = new_sizes;
    mf->local_cap = cap;
  }
  int slot = mf->local_count++;
  mf->local_vars[slot] = var;
  mf->local_types[slot] = type;
  mf->local_sizes[slot] = size;
  return slot + 1;
}

int mir_get_or_add_local_slot(MirFunction *mf, LVar *var, Type *type, int size) {
  if (!mf || !var)
    error("invalid local variable [in mir_get_or_add_local_slot]");
  for (int i = 0; i < mf->local_count; i++) {
    if (mf->local_vars[i] == var)
      return i + 1;
  }
  return mir_add_local_slot(mf, var, type, size);
}

int mir_new_block(MirFunction *mf) {
  if (!mf)
    return -1;
  if (mf->block_count >= mf->block_cap) {
    int cap = mf->block_cap ? mf->block_cap * 2 : 4;
    MirBasicBlock *new_buf = realloc(mf->blocks, sizeof(MirBasicBlock) * cap);
    if (!new_buf)
      error("memory allocation failed [in mir_new_block]");
    mf->blocks = new_buf;
    mf->block_cap = cap;
  }
  int id = mf->block_count++;
  MirBasicBlock *bb = &mf->blocks[id];
  memset(bb, 0, sizeof(*bb));
  bb->id = id;
  bb->label = MIR_INVALID_LABEL;
  bb->succ[0] = -1;
  bb->succ[1] = -1;
  bb->succ_count = 0;
  bb->preds = NULL;
  bb->pred_count = 0;
  bb->pred_cap = 0;
  mf->current_block = id;
  return id;
}

static int is_terminator(MirOp op) {
  return op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET;
}

void mir_emit(MirFunction *mf, const MirInst *inst) {
  if (!mf || !inst)
    return;
  if (mf->block_count == 0 || mf->current_block < 0) {
    mir_new_block(mf);
  } else if (inst->op == MIR_OP_LABEL && mf->blocks[mf->current_block].inst_len > 0) {
    mir_new_block(mf);
  }
  MirBasicBlock *bb = &mf->blocks[mf->current_block];
  if (inst->op == MIR_OP_LABEL)
    bb->label = inst->label;
  ensure_bb_inst_capacity(bb, bb->inst_len + 1);
  bb->insts[bb->inst_len++] = *inst;
  if (is_terminator(inst->op))
    mf->current_block = -1;
}

static void add_pred(MirBasicBlock *bb, int pred) {
  for (int i = 0; i < bb->pred_count; i++) {
    if (bb->preds[i] == pred)
      return;
  }
  if (bb->pred_count >= bb->pred_cap) {
    int cap = bb->pred_cap ? bb->pred_cap * 2 : 4;
    int *new_preds = realloc(bb->preds, sizeof(int) * cap);
    if (!new_preds)
      error("memory allocation failed [in add_pred]");
    bb->preds = new_preds;
    bb->pred_cap = cap;
  }
  bb->preds[bb->pred_count++] = pred;
}

static void add_succ(MirFunction *mf, int from, int to) {
  if (from < 0 || from >= mf->block_count || to < 0 || to >= mf->block_count)
    return;
  MirBasicBlock *bb = &mf->blocks[from];
  for (int i = 0; i < bb->succ_count; i++) {
    if (bb->succ[i] == to)
      return;
  }
  if (bb->succ_count >= 2)
    error("too many CFG successors");
  bb->succ[bb->succ_count++] = to;
  add_pred(&mf->blocks[to], from);
}

void mir_finalize_cfg(MirFunction *mf) {
  if (!mf || mf->block_count <= 0)
    return;
  int *label_to_block = malloc(sizeof(int) * (mf->next_label > 0 ? mf->next_label : 1));
  if (!label_to_block)
    error("memory allocation failed [in mir_finalize_cfg]");
  for (int l = 0; l < mf->next_label; l++)
    label_to_block[l] = -1;
  for (int i = 0; i < mf->block_count; i++) {
    MirBasicBlock *bb = &mf->blocks[i];
    bb->succ_count = 0;
    bb->pred_count = 0;
    if (bb->label >= 0 && bb->label < mf->next_label)
      label_to_block[bb->label] = i;
  }
  for (int i = 0; i < mf->block_count; i++) {
    MirBasicBlock *bb = &mf->blocks[i];
    if (bb->inst_len <= 0)
      continue;
    MirInst *term = &bb->insts[bb->inst_len - 1];
    if (term->op == MIR_OP_JMP) {
      if (term->label >= 0 && term->label < mf->next_label)
        add_succ(mf, i, label_to_block[term->label]);
    } else if (term->op == MIR_OP_JZ || term->op == MIR_OP_JCC) {
      if (term->label >= 0 && term->label < mf->next_label)
        add_succ(mf, i, label_to_block[term->label]);
      if (i + 1 < mf->block_count)
        add_succ(mf, i, i + 1);
    } else if (term->op != MIR_OP_RET && i + 1 < mf->block_count) {
      add_succ(mf, i, i + 1);
    }
  }
  free(label_to_block);
}

void mir_linearize(MirFunction *mf) {
  if (!mf || mf->block_count <= 1)
    return;
  int total = 0;
  for (int i = 0; i < mf->block_count; i++)
    total += mf->blocks[i].inst_len;
  MirInst *insts = malloc(sizeof(MirInst) * (total > 0 ? total : 1));
  if (!insts)
    error("memory allocation failed [in mir_linearize]");
  int n = 0;
  for (int i = 0; i < mf->block_count; i++) {
    MirBasicBlock *bb = &mf->blocks[i];
    for (int j = 0; j < bb->inst_len; j++)
      insts[n++] = bb->insts[j];
    free(bb->insts);
    free(bb->preds);
    for (int j = 0; j < bb->phi_count; j++) {
      free(bb->phis[j].incoming_block);
      free(bb->phis[j].incoming_value);
    }
    free(bb->phis);
  }
  mf->blocks[0].insts = insts;
  mf->blocks[0].inst_len = n;
  mf->blocks[0].inst_cap = n;
  mf->blocks[0].preds = NULL;
  mf->blocks[0].pred_count = 0;
  mf->blocks[0].pred_cap = 0;
  mf->blocks[0].phis = NULL;
  mf->blocks[0].phi_count = 0;
  mf->blocks[0].phi_cap = 0;
  mf->blocks[0].succ_count = 0;
  mf->block_count = 1;
  mf->current_block = 0;
}

// MirFunction の内容を人間が読める形式で出力する
void mir_dump(FILE *out, const MirFunction *mf) {
  if (!out || !mf)
    return;

  int total_insts = 0;
  for (int i = 0; i < mf->block_count; i++)
    total_insts += mf->blocks[i].inst_len;
  if (mf->fn) {
    fprintf(out, "MIR function %.*s (insts=%d, next_vreg=%d, next_label=%d)\n", mf->fn->len, mf->fn->name, total_insts,
            mf->next_vreg, mf->next_label);
  } else {
    fprintf(out, "MIR function <null> (insts=%d, next_vreg=%d, next_label=%d)\n", total_insts, mf->next_vreg,
            mf->next_label);
  }

  if (mf->block_count == 0)
    return;
  for (int bi = 0; bi < mf->block_count; bi++) {
    const MirBasicBlock *bb = &mf->blocks[bi];
    fprintf(out, "  block %d:", bi);
    for (int p = 0; p < bb->phi_count; p++)
      fprintf(out, " v%d=phi(var%d)", bb->phis[p].dst, bb->phis[p].var);
    fprintf(out, "\n");
    for (int i = 0; i < bb->inst_len; i++) {
      const MirInst *in = &bb->insts[i];
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
}

void mir_free(MirFunction *mf) {
  if (!mf)
    return;
  for (int i = 0; i < mf->block_count; i++) {
    MirBasicBlock *bb = &mf->blocks[i];
    for (int j = 0; j < bb->phi_count; j++) {
      free(bb->phis[j].incoming_block);
      free(bb->phis[j].incoming_value);
    }
    free(bb->phis);
    free(bb->insts);
    free(bb->preds);
  }
  free(mf->blocks);
  free(mf->local_vars);
  free(mf->local_types);
  free(mf->local_sizes);
  mf->blocks = NULL;
  mf->local_vars = NULL;
  mf->local_types = NULL;
  mf->local_sizes = NULL;
  mf->block_count = 0;
  mf->block_cap = 0;
  mf->local_count = 0;
  mf->local_cap = 0;
  mf->next_vreg = 0;
  mf->next_label = 0;
  mf->current_block = -1;
  mf->fn = NULL;
}
