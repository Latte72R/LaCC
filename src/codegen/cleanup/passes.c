#include "../bitset.h"
#include "diagnostics.h"
#include "internal.h"

#include <stdlib.h>
#include <string.h>

static int is_control_barrier(MirOp op) {
  return op == MIR_OP_LABEL || op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET;
}
static int same_scalar_type(Type *a, Type *b) {
  if (!a || !b)
    return 0;
  return a->ty == b->ty && a->is_unsigned == b->is_unsigned;
}

static int is_integer_scalar_type(Type *type) {
  if (!type)
    return 0;
  switch (type->ty) {
  case TY_BOOL:
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
  case TY_LONGLONG:
    return 1;
  default:
    return 0;
  }
}

static int is_noop_scalar_cast(Type *from, Type *to) {
  if (!from || !to)
    return 0;
  if (same_scalar_type(from, to))
    return 1;
  if (!is_integer_scalar_type(from) || !is_integer_scalar_type(to))
    return 0;
  return type_size(from) == type_size(to);
}

static int get_single_def_imm(VReg v, int vreg_count, const int *def_count, const MirOp *def_op, const long *def_imm,
                              long *out) {
  if (v < 0 || v >= vreg_count || !def_count || !def_op || !def_imm)
    return 0;
  if (def_count[v] != 1 || def_op[v] != MIR_OP_IMM)
    return 0;
  if (out)
    *out = def_imm[v];
  return 1;
}

static int try_get_identity_copy_source(const MirInst *in, int vreg_count, const int *def_count, const MirOp *def_op,
                                        const long *def_imm, VReg *out_src) {
  if (!in || !out_src)
    return 0;
  if (in->src1 < 0 || in->src1 >= vreg_count || in->src2 < 0 || in->src2 >= vreg_count)
    return 0;

  long imm1 = 0;
  long imm2 = 0;
  int src1_imm = get_single_def_imm(in->src1, vreg_count, def_count, def_op, def_imm, &imm1);
  int src2_imm = get_single_def_imm(in->src2, vreg_count, def_count, def_op, def_imm, &imm2);

  switch (in->op) {
  case MIR_OP_ADD:
    if (src1_imm && imm1 == 0) {
      *out_src = in->src2;
      return 1;
    }
    if (src2_imm && imm2 == 0) {
      *out_src = in->src1;
      return 1;
    }
    return 0;
  case MIR_OP_SUB:
  case MIR_OP_SHL:
  case MIR_OP_SHR:
    if (src2_imm && imm2 == 0) {
      *out_src = in->src1;
      return 1;
    }
    return 0;
  case MIR_OP_MUL:
    if (src1_imm && imm1 == 1) {
      *out_src = in->src2;
      return 1;
    }
    if (src2_imm && imm2 == 1) {
      *out_src = in->src1;
      return 1;
    }
    return 0;
  case MIR_OP_BITOR:
  case MIR_OP_BITXOR:
    if (src1_imm && imm1 == 0) {
      *out_src = in->src2;
      return 1;
    }
    if (src2_imm && imm2 == 0) {
      *out_src = in->src1;
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

static long normalize_const_by_type(Type *type, long value) {
  if (!type)
    return value;
  switch (type->ty) {
  case TY_BOOL:
    return value ? 1 : 0;
  case TY_CHAR:
    if (type->is_unsigned)
      return (long)(unsigned char)value;
    return (long)(signed char)value;
  case TY_SHORT:
    if (type->is_unsigned)
      return (long)(unsigned short)value;
    return (long)(short)value;
  case TY_INT:
    if (type->is_unsigned)
      return (long)(unsigned int)value;
    return (long)(int)value;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
  default:
    if (type->is_unsigned)
      return (long)(unsigned long)value;
    return value;
  }
}

static int fold_binary_const(MirOp op, Type *type, long lhs, long rhs, long *out) {
  if (!out)
    return 0;
  if (type && type->ty == TY_BOOL) {
    switch (op) {
    case MIR_OP_ADD:
    case MIR_OP_SUB:
    case MIR_OP_MUL:
    case MIR_OP_BITAND:
    case MIR_OP_BITOR:
    case MIR_OP_BITXOR:
      return 0;
    default:
      break;
    }
  }

  long l = normalize_const_by_type(type, lhs);
  long r = normalize_const_by_type(type, rhs);
  unsigned long ul = (unsigned long)l;
  unsigned long ur = (unsigned long)r;

  switch (op) {
  case MIR_OP_ADD:
    *out = (long)(ul + ur);
    return 1;
  case MIR_OP_SUB:
    *out = (long)(ul - ur);
    return 1;
  case MIR_OP_MUL:
    *out = (long)(ul * ur);
    return 1;
  case MIR_OP_BITAND:
    *out = (long)(ul & ur);
    return 1;
  case MIR_OP_BITOR:
    *out = (long)(ul | ur);
    return 1;
  case MIR_OP_BITXOR:
    *out = (long)(ul ^ ur);
    return 1;
  case MIR_OP_EQ:
    *out = (l == r) ? 1 : 0;
    return 1;
  case MIR_OP_NE:
    *out = (l != r) ? 1 : 0;
    return 1;
  case MIR_OP_LT:
    if (type && type->is_unsigned)
      *out = (ul < ur) ? 1 : 0;
    else
      *out = (l < r) ? 1 : 0;
    return 1;
  case MIR_OP_LE:
    if (type && type->is_unsigned)
      *out = (ul <= ur) ? 1 : 0;
    else
      *out = (l <= r) ? 1 : 0;
    return 1;
  default:
    return 0;
  }
}

static int fold_unary_const(MirOp op, Type *type, long src, long *out) {
  if (!out)
    return 0;
  if (type && type->ty == TY_BOOL)
    return 0;
  long v = src;
  switch (op) {
  case MIR_OP_BITNOT:
    *out = (long)(~(unsigned long)v);
    return 1;
  default:
    return 0;
  }
}

static int resolve_copy_source(const VReg *copy_src, int vreg_count, int v) {
  if (!copy_src || v < 0 || v >= vreg_count)
    return v;
  int cur = v;
  int guard = 0;
  while (cur >= 0 && cur < vreg_count && copy_src[cur] != MIR_INVALID_VREG && copy_src[cur] != cur) {
    cur = copy_src[cur];
    if (++guard > vreg_count)
      break;
  }
  return cur;
}

static void rewrite_vreg_use(VReg *use, const VReg *copy_src, int vreg_count) {
  if (!use || !copy_src || *use < 0 || *use >= vreg_count)
    return;
  *use = resolve_copy_source(copy_src, vreg_count, *use);
}

static void rewrite_inst_uses(MirInst *in, const VReg *copy_src, int vreg_count) {
  if (!in || !copy_src || vreg_count <= 0)
    return;
  rewrite_vreg_use(&in->src1, copy_src, vreg_count);
  rewrite_vreg_use(&in->src2, copy_src, vreg_count);
  if (in->op == MIR_OP_CALL) {
    for (int a = 0; a < in->argc; a++)
      rewrite_vreg_use(&in->args[a], copy_src, vreg_count);
  }
}

static void for_each_inst_use(const MirInst *in, int vreg_count, void (*fn)(VReg v, void *ctx), void *ctx) {
  if (!in || !fn || vreg_count <= 0)
    return;
  if (in->src1 >= 0 && in->src1 < vreg_count)
    fn(in->src1, ctx);
  if (in->src2 >= 0 && in->src2 < vreg_count)
    fn(in->src2, ctx);
  if (in->op == MIR_OP_CALL) {
    for (int a = 0; a < in->argc; a++) {
      VReg arg = in->args[a];
      if (arg >= 0 && arg < vreg_count)
        fn(arg, ctx);
    }
  }
}

static void clear_copy_relations(VReg *copy_src, int vreg_count) {
  if (!copy_src || vreg_count <= 0)
    return;
  for (int v = 0; v < vreg_count; v++)
    copy_src[v] = MIR_INVALID_VREG;
}

static void kill_copy_relations_of(VReg *copy_src, int vreg_count, VReg dst) {
  if (!copy_src || vreg_count <= 0 || dst < 0 || dst >= vreg_count)
    return;
  copy_src[dst] = MIR_INVALID_VREG;
  for (int v = 0; v < vreg_count; v++) {
    if (copy_src[v] == MIR_INVALID_VREG)
      continue;
    if (resolve_copy_source(copy_src, vreg_count, copy_src[v]) == dst)
      copy_src[v] = MIR_INVALID_VREG;
  }
}

typedef struct {
  int *use_count;
  int delta;
} UseCountDeltaCtx;

static void apply_use_count_delta(VReg v, void *ctx_void) {
  UseCountDeltaCtx *ctx = ctx_void;
  if (!ctx || !ctx->use_count)
    return;
  if (ctx->delta > 0) {
    ctx->use_count[v] += ctx->delta;
  } else if (ctx->delta < 0 && ctx->use_count[v] > 0) {
    ctx->use_count[v] += ctx->delta;
    if (ctx->use_count[v] < 0)
      ctx->use_count[v] = 0;
  }
}

static void add_inst_uses(const MirInst *in, int *use_count, int vreg_count) {
  if (!in || !use_count || vreg_count <= 0)
    return;
  UseCountDeltaCtx ctx;
  ctx.use_count = use_count;
  ctx.delta = +1;
  for_each_inst_use(in, vreg_count, apply_use_count_delta, &ctx);
}

static void remove_inst_uses(const MirInst *in, int *use_count, int vreg_count) {
  if (!in || !use_count || vreg_count <= 0)
    return;
  UseCountDeltaCtx ctx;
  ctx.use_count = use_count;
  ctx.delta = -1;
  for_each_inst_use(in, vreg_count, apply_use_count_delta, &ctx);
}

static int is_trivial_dead_def_candidate(MirOp op) {
  switch (op) {
  case MIR_OP_MOV:
  case MIR_OP_IMM:
  case MIR_OP_CAST:
  case MIR_OP_LOAD_LOCAL:
  case MIR_OP_ADD:
  case MIR_OP_SUB:
  case MIR_OP_MUL:
  case MIR_OP_EQ:
  case MIR_OP_NE:
  case MIR_OP_LT:
  case MIR_OP_LE:
  case MIR_OP_BITAND:
  case MIR_OP_BITOR:
  case MIR_OP_BITXOR:
  case MIR_OP_BITNOT:
  case MIR_OP_ADDR_LOCAL:
  case MIR_OP_ADDR_SYMBOL:
  case MIR_OP_ADDR_FUNC:
  case MIR_OP_ADDR_STRING:
  case MIR_OP_ADDR_ARRAY:
  case MIR_OP_ADDR_STRUCT_LITERAL:
    return 1;
  default:
    return 0;
  }
}

void run_cleanup_prune_unreferenced_labels(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_label <= 0)
    return;

  unsigned char *referenced = calloc(next_label, sizeof(unsigned char));
  if (!referenced)
    error("memory allocation failed [in cleanup prune labels]");

  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (in->op != MIR_OP_JMP && in->op != MIR_OP_JZ && in->op != MIR_OP_JCC)
      continue;
    if (in->label < 0 || in->label >= next_label)
      continue;
    referenced[in->label] = 1;
  }

  int out = 0;
  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (in->op == MIR_OP_LABEL) {
      if (in->label >= 0 && in->label < next_label && !referenced[in->label])
        continue;
    }
    if (out != i)
      (*insts)[out] = (*insts)[i];
    out++;
  }
  *inst_len = out;
  *inst_cap = out;

  free(referenced);
}

static int false_cc_for_compare_op(MirOp op, MirCondCode *cc) {
  if (!cc)
    return 0;
  switch (op) {
  case MIR_OP_EQ:
    *cc = MIR_CC_NE;
    return 1;
  case MIR_OP_NE:
    *cc = MIR_CC_EQ;
    return 1;
  case MIR_OP_LT:
    *cc = MIR_CC_GE;
    return 1;
  case MIR_OP_LE:
    *cc = MIR_CC_GT;
    return 1;
  default:
    return 0;
  }
}

void run_cleanup_fuse_compare_jz(MirInst **insts, int *inst_len) {
  if (!insts || !*insts || !inst_len || *inst_len <= 1)
    return;

  for (int i = 0; i + 1 < *inst_len; i++) {
    MirInst *cmp = &(*insts)[i];
    MirInst *jz = &(*insts)[i + 1];
    if (jz->op != MIR_OP_JZ)
      continue;
    if (cmp->dst == MIR_INVALID_VREG || jz->src1 != cmp->dst)
      continue;

    MirCondCode cc;
    if (!false_cc_for_compare_op(cmp->op, &cc))
      continue;
    if (cmp->src1 == MIR_INVALID_VREG || cmp->src2 == MIR_INVALID_VREG)
      continue;

    jz->op = MIR_OP_JCC;
    jz->src1 = cmp->src1;
    jz->src2 = cmp->src2;
    jz->imm = cc;
    jz->type = cmp->type;
  }
}

static int is_commutative_imm_rhs_op(MirOp op) {
  switch (op) {
  case MIR_OP_ADD:
  case MIR_OP_MUL:
  case MIR_OP_BITAND:
  case MIR_OP_BITOR:
  case MIR_OP_BITXOR:
  case MIR_OP_EQ:
  case MIR_OP_NE:
    return 1;
  default:
    return 0;
  }
}

void run_cleanup_canonicalize_commutative_imm_rhs(MirInst **insts, int *inst_len, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *def_count = calloc(next_vreg, sizeof(int));
  MirOp *def_op = calloc(next_vreg, sizeof(MirOp));
  long *def_imm = calloc(next_vreg, sizeof(long));
  if (!def_count || !def_op || !def_imm)
    error("memory allocation failed [in mem2reg commutative canonicalize setup]");

  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (in->dst < 0 || in->dst >= next_vreg)
      continue;
    def_count[in->dst]++;
    def_op[in->dst] = in->op;
    def_imm[in->dst] = in->imm;
  }

  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (!is_commutative_imm_rhs_op(in->op))
      continue;
    if (in->src1 < 0 || in->src1 >= next_vreg || in->src2 < 0 || in->src2 >= next_vreg)
      continue;

    int src1_is_imm = get_single_def_imm(in->src1, next_vreg, def_count, def_op, def_imm, NULL);
    int src2_is_imm = get_single_def_imm(in->src2, next_vreg, def_count, def_op, def_imm, NULL);
    if (src1_is_imm && !src2_is_imm) {
      VReg t = in->src1;
      in->src1 = in->src2;
      in->src2 = t;
    }
  }

  free(def_imm);
  free(def_op);
  free(def_count);
}

static void clear_inst_to_nop(MirInst *in) {
  if (!in)
    return;
  in->op = MIR_OP_NOP;
  in->dst = MIR_INVALID_VREG;
  in->src1 = MIR_INVALID_VREG;
  in->src2 = MIR_INVALID_VREG;
  in->imm = 0;
  in->offset = 0;
  in->label = MIR_INVALID_LABEL;
  in->var = NULL;
  in->call_fn = NULL;
  in->argc = 0;
  for (int a = 0; a < MAX_FUNC_PARAMS; a++)
    in->args[a] = MIR_INVALID_VREG;
}

static void run_cleanup_simplify_jumps_to_next_label(MirInst **insts, int *inst_len) {
  if (!insts || !*insts || !inst_len || *inst_len <= 1)
    return;

  for (int i = 0; i + 1 < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    MirInst *next = &(*insts)[i + 1];
    if (next->op != MIR_OP_LABEL)
      continue;
    if (in->op != MIR_OP_JMP && in->op != MIR_OP_JZ && in->op != MIR_OP_JCC)
      continue;
    if (in->label != next->label)
      continue;
    clear_inst_to_nop(in);
  }
}

static void run_cleanup_remove_nops(MirInst **insts, int *inst_len, int *inst_cap) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0)
    return;

  int out = 0;
  for (int i = 0; i < *inst_len; i++) {
    if ((*insts)[i].op == MIR_OP_NOP)
      continue;
    if (out != i)
      (*insts)[out] = (*insts)[i];
    out++;
  }
  *inst_len = out;
  if (inst_cap)
    *inst_cap = out;
}

static int eval_const_jcc(long cc, Type *type, long lhs, long rhs) {
  unsigned long ulhs = (unsigned long)lhs;
  unsigned long urhs = (unsigned long)rhs;
  switch ((MirCondCode)cc) {
  case MIR_CC_EQ:
    return lhs == rhs;
  case MIR_CC_NE:
    return lhs != rhs;
  case MIR_CC_LT:
    return (type && type->is_unsigned) ? (ulhs < urhs) : (lhs < rhs);
  case MIR_CC_LE:
    return (type && type->is_unsigned) ? (ulhs <= urhs) : (lhs <= rhs);
  case MIR_CC_GT:
    return (type && type->is_unsigned) ? (ulhs > urhs) : (lhs > rhs);
  case MIR_CC_GE:
    return (type && type->is_unsigned) ? (ulhs >= urhs) : (lhs >= rhs);
  default:
    return 0;
  }
}

static int *build_label_to_inst_map(const MirInst *insts, int inst_len, int next_label, const char *pass_name) {
  if (!insts || inst_len <= 0 || next_label <= 0)
    return NULL;

  int *label_to_inst = malloc(sizeof(int) * next_label);
  if (!label_to_inst)
    error("memory allocation failed [in %s labels]", pass_name ? pass_name : "cleanup");

  for (int l = 0; l < next_label; l++)
    label_to_inst[l] = -1;
  for (int i = 0; i < inst_len; i++) {
    if (insts[i].op != MIR_OP_LABEL)
      continue;
    int l = insts[i].label;
    if (l >= 0 && l < next_label)
      label_to_inst[l] = i;
  }
  return label_to_inst;
}

static void run_cleanup_eliminate_dead_defs(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *use_count = calloc(next_vreg, sizeof(int));
  unsigned char *dead = calloc(*inst_len, sizeof(unsigned char));
  if (!use_count || !dead)
    error("memory allocation failed [in cleanup dce setup]");

  for (int i = 0; i < *inst_len; i++)
    add_inst_uses(&(*insts)[i], use_count, next_vreg);

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int i = 0; i < *inst_len; i++) {
      MirInst *in = &(*insts)[i];
      if (dead[i])
        continue;
      if (!is_trivial_dead_def_candidate(in->op))
        continue;
      if (in->dst < 0 || in->dst >= next_vreg)
        continue;
      if (use_count[in->dst] != 0)
        continue;

      dead[i] = 1;
      changed = 1;
      remove_inst_uses(in, use_count, next_vreg);
    }
  }

  int out = 0;
  for (int i = 0; i < *inst_len; i++) {
    if (dead[i])
      continue;
    if (out != i)
      (*insts)[out] = (*insts)[i];
    out++;
  }
  *inst_len = out;
  if (inst_cap)
    *inst_cap = out;

  free(use_count);
  free(dead);
}

void run_cleanup_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *def_count = calloc(next_vreg, sizeof(int));
  MirOp *def_op = calloc(next_vreg, sizeof(MirOp));
  Type **def_type = calloc(next_vreg, sizeof(Type *));
  long *def_imm = calloc(next_vreg, sizeof(long));
  if (!def_count || !def_op || !def_type || !def_imm)
    error("memory allocation failed [in mem2reg copy-prop def info]");
  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (in->dst < 0 || in->dst >= next_vreg)
      continue;
    def_count[in->dst]++;
    def_op[in->dst] = in->op;
    def_type[in->dst] = in->type;
    def_imm[in->dst] = in->imm;
  }

  VReg *copy_src = malloc(sizeof(VReg) * next_vreg);
  if (!copy_src)
    error("memory allocation failed [in mem2reg copy-prop setup]");
  clear_copy_relations(copy_src, next_vreg);

  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    rewrite_inst_uses(in, copy_src, next_vreg);

    if (in->dst >= 0 && in->dst < next_vreg)
      kill_copy_relations_of(copy_src, next_vreg, in->dst);

    if (in->op == MIR_OP_MOV && in->dst >= 0 && in->dst < next_vreg && in->src1 >= 0 && in->src1 < next_vreg &&
        in->dst != in->src1) {
      copy_src[in->dst] = in->src1;
    }

    if (in->op == MIR_OP_CAST && in->dst >= 0 && in->dst < next_vreg && in->src1 >= 0 && in->src1 < next_vreg &&
        in->dst != in->src1 && def_count[in->src1] == 1 && is_noop_scalar_cast(def_type[in->src1], in->type) &&
        def_op[in->src1] != MIR_OP_NOP) {
      copy_src[in->dst] = in->src1;
    }

    VReg identity_src = MIR_INVALID_VREG;
    if (in->dst >= 0 && in->dst < next_vreg &&
        try_get_identity_copy_source(in, next_vreg, def_count, def_op, def_imm, &identity_src) &&
        identity_src != MIR_INVALID_VREG && identity_src != in->dst) {
      copy_src[in->dst] = identity_src;
    }

    if (is_control_barrier(in->op))
      clear_copy_relations(copy_src, next_vreg);
  }

  free(copy_src);
  free(def_imm);
  free(def_type);
  free(def_op);
  free(def_count);
  run_cleanup_eliminate_dead_defs(insts, inst_len, inst_cap, next_vreg);
}

void run_cleanup_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg) {
  run_cleanup_eliminate_dead_defs(insts, inst_len, inst_cap, next_vreg);
}

void run_cleanup_prune_unreachable_blocks(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0)
    return;

  int n = *inst_len;
  int *label_to_inst = build_label_to_inst_map(*insts, n, next_label, "cleanup prune unreachable");

  unsigned char *visited = calloc(n, sizeof(unsigned char));
  int *queue = malloc(sizeof(int) * n);
  if (!visited || !queue)
    error("memory allocation failed [in cleanup prune unreachable work]");
  int qh = 0;
  int qt = 0;
  if (n > 0) {
    visited[0] = 1;
    queue[qt++] = 0;
  }

  while (qh < qt) {
    int i = queue[qh++];
    MirInst *in = &(*insts)[i];
    int succs[2] = {-1, -1};
    int succ_cnt = 0;
    if (in->op == MIR_OP_JMP) {
      if (label_to_inst && in->label >= 0 && in->label < next_label)
        succs[succ_cnt++] = label_to_inst[in->label];
    } else if (in->op == MIR_OP_JZ || in->op == MIR_OP_JCC) {
      if (i + 1 < n)
        succs[succ_cnt++] = i + 1;
      if (label_to_inst && in->label >= 0 && in->label < next_label)
        succs[succ_cnt++] = label_to_inst[in->label];
    } else if (in->op != MIR_OP_RET) {
      if (i + 1 < n)
        succs[succ_cnt++] = i + 1;
    }
    for (int s = 0; s < succ_cnt; s++) {
      int si = succs[s];
      if (si < 0 || si >= n || visited[si])
        continue;
      visited[si] = 1;
      queue[qt++] = si;
    }
  }

  int out = 0;
  for (int i = 0; i < n; i++) {
    if (!visited[i])
      continue;
    if (out != i)
      (*insts)[out] = (*insts)[i];
    out++;
  }
  *inst_len = out;
  *inst_cap = out;

  free(queue);
  free(visited);
  free(label_to_inst);
}

typedef struct {
  int start;
  int end;
  int succ[2];
  int succ_cnt;
} CleanupBlock;

static int is_cfg_terminator(MirOp op) {
  return op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET;
}

static void add_cfg_succ(CleanupBlock *bb, int succ) {
  if (!bb || succ < 0)
    return;
  for (int i = 0; i < bb->succ_cnt; i++) {
    if (bb->succ[i] == succ)
      return;
  }
  if (bb->succ_cnt < 2)
    bb->succ[bb->succ_cnt++] = succ;
}

static void build_cleanup_cfg(const MirFunction *mf, CleanupBlock **out_blocks, int *out_block_len) {
  if (!mf || !out_blocks || !out_block_len)
    error("invalid cfg builder args [in cleanup cfg]");

  int n = mf->blocks[0].inst_len;
  unsigned char *is_block_start = calloc(n > 0 ? n : 1, sizeof(unsigned char));
  if (!is_block_start)
    error("memory allocation failed [in cleanup cfg starts]");

  if (n > 0)
    is_block_start[0] = 1;
  for (int i = 1; i < n; i++) {
    if (mf->blocks[0].insts[i].op == MIR_OP_LABEL || is_cfg_terminator(mf->blocks[0].insts[i - 1].op))
      is_block_start[i] = 1;
  }

  int block_len = 0;
  for (int i = 0; i < n; i++) {
    if (is_block_start[i])
      block_len++;
  }
  if (block_len <= 0)
    block_len = 1;

  CleanupBlock *blocks = calloc(block_len, sizeof(CleanupBlock));
  int *label_to_block = malloc(sizeof(int) * (mf->next_label > 0 ? mf->next_label : 1));
  if (!blocks || !label_to_block)
    error("memory allocation failed [in cleanup cfg alloc]");

  for (int l = 0; l < mf->next_label; l++)
    label_to_block[l] = -1;

  int b = 0;
  for (int i = 0; i < n; i++) {
    if (!is_block_start[i])
      continue;
    blocks[b].start = i;
    blocks[b].end = n;
    blocks[b].succ[0] = -1;
    blocks[b].succ[1] = -1;
    blocks[b].succ_cnt = 0;
    if (b > 0)
      blocks[b - 1].end = i;
    b++;
  }

  for (int bi = 0; bi < block_len; bi++) {
    MirInst *first = &mf->blocks[0].insts[blocks[bi].start];
    if (first->op == MIR_OP_LABEL && first->label >= 0 && first->label < mf->next_label)
      label_to_block[first->label] = bi;
  }

  for (int bi = 0; bi < block_len; bi++) {
    CleanupBlock *bb = &blocks[bi];
    if (bb->start >= bb->end)
      continue;
    MirInst *term = &mf->blocks[0].insts[bb->end - 1];
    int fallthrough = (bi + 1 < block_len) ? (bi + 1) : -1;
    if (term->op == MIR_OP_JMP) {
      if (term->label >= 0 && term->label < mf->next_label)
        add_cfg_succ(bb, label_to_block[term->label]);
      continue;
    }
    if (term->op == MIR_OP_JZ || term->op == MIR_OP_JCC) {
      if (term->label >= 0 && term->label < mf->next_label)
        add_cfg_succ(bb, label_to_block[term->label]);
      add_cfg_succ(bb, fallthrough);
      continue;
    }
    if (term->op == MIR_OP_RET)
      continue;
    add_cfg_succ(bb, fallthrough);
  }

  free(is_block_start);
  free(label_to_block);
  *out_blocks = blocks;
  *out_block_len = block_len;
}

typedef enum {
  SCCP_LATTICE_UNDEF = 0,
  SCCP_LATTICE_CONST,
  SCCP_LATTICE_OVERDEF,
} SccpLatticeKind;

static long canonicalize_typed_imm(Type *type, long imm) { return normalize_const_by_type(type, imm); }

static int sccp_merge_lattice(SccpLatticeKind *kind, long *value, SccpLatticeKind next_kind, long next_value) {
  if (!kind || !value)
    return 0;
  if (next_kind == SCCP_LATTICE_UNDEF)
    return 0;
  if (*kind == SCCP_LATTICE_UNDEF) {
    *kind = next_kind;
    if (next_kind == SCCP_LATTICE_CONST)
      *value = next_value;
    return 1;
  }
  if (*kind == SCCP_LATTICE_CONST && next_kind == SCCP_LATTICE_CONST) {
    if (*value == next_value)
      return 0;
    *kind = SCCP_LATTICE_OVERDEF;
    return 1;
  }
  if (*kind != SCCP_LATTICE_OVERDEF) {
    *kind = SCCP_LATTICE_OVERDEF;
    return 1;
  }
  return 0;
}

static SccpLatticeKind sccp_vreg_kind(VReg v, int vreg_count, const SccpLatticeKind *kinds, const long *values,
                                      long *out_value) {
  if (v < 0 || v >= vreg_count || !kinds || !values)
    return SCCP_LATTICE_OVERDEF;
  if (kinds[v] == SCCP_LATTICE_CONST && out_value)
    *out_value = values[v];
  return kinds[v];
}

static int sccp_eval_inst_result(const MirInst *in, int vreg_count, const SccpLatticeKind *kinds, const long *values,
                                 SccpLatticeKind *out_kind, long *out_value) {
  if (!in || !out_kind || !out_value || in->dst < 0 || in->dst >= vreg_count)
    return 0;

  *out_kind = SCCP_LATTICE_OVERDEF;
  *out_value = 0;

  if (in->op == MIR_OP_IMM) {
    *out_kind = SCCP_LATTICE_CONST;
    *out_value = canonicalize_typed_imm(in->type, in->imm);
    return 1;
  }

  if (in->op == MIR_OP_MOV || in->op == MIR_OP_CAST) {
    long src = 0;
    SccpLatticeKind sk = sccp_vreg_kind(in->src1, vreg_count, kinds, values, &src);
    if (sk == SCCP_LATTICE_CONST) {
      *out_kind = SCCP_LATTICE_CONST;
      *out_value = (in->op == MIR_OP_CAST) ? canonicalize_typed_imm(in->type, src) : src;
    } else {
      *out_kind = sk;
    }
    return 1;
  }

  if (in->op == MIR_OP_BITNOT) {
    long src = 0;
    SccpLatticeKind sk = sccp_vreg_kind(in->src1, vreg_count, kinds, values, &src);
    if (sk == SCCP_LATTICE_CONST) {
      long folded = 0;
      if (fold_unary_const(in->op, in->type, src, &folded)) {
        *out_kind = SCCP_LATTICE_CONST;
        *out_value = canonicalize_typed_imm(in->type, folded);
      } else {
        *out_kind = SCCP_LATTICE_OVERDEF;
      }
    } else {
      *out_kind = sk;
    }
    return 1;
  }

  switch (in->op) {
  case MIR_OP_ADD:
  case MIR_OP_SUB:
  case MIR_OP_MUL:
  case MIR_OP_EQ:
  case MIR_OP_NE:
  case MIR_OP_LT:
  case MIR_OP_LE:
  case MIR_OP_BITAND:
  case MIR_OP_BITOR:
  case MIR_OP_BITXOR: {
    long lhs = 0;
    long rhs = 0;
    SccpLatticeKind lk = sccp_vreg_kind(in->src1, vreg_count, kinds, values, &lhs);
    SccpLatticeKind rk = sccp_vreg_kind(in->src2, vreg_count, kinds, values, &rhs);
    if (lk == SCCP_LATTICE_CONST && rk == SCCP_LATTICE_CONST) {
      long folded = 0;
      if (fold_binary_const(in->op, in->type, lhs, rhs, &folded)) {
        *out_kind = SCCP_LATTICE_CONST;
        *out_value = canonicalize_typed_imm(in->type, folded);
      } else {
        *out_kind = SCCP_LATTICE_OVERDEF;
      }
    } else if (lk == SCCP_LATTICE_OVERDEF || rk == SCCP_LATTICE_OVERDEF) {
      *out_kind = SCCP_LATTICE_OVERDEF;
    } else {
      *out_kind = SCCP_LATTICE_UNDEF;
    }
    return 1;
  }
  case MIR_OP_SHL:
  case MIR_OP_SHR: {
    long lhs = 0;
    long rhs = 0;
    SccpLatticeKind lk = sccp_vreg_kind(in->src1, vreg_count, kinds, values, &lhs);
    SccpLatticeKind rk = sccp_vreg_kind(in->src2, vreg_count, kinds, values, &rhs);
    if (lk == SCCP_LATTICE_CONST && rk == SCCP_LATTICE_CONST && rhs >= 0 && rhs <= 63) {
      unsigned int sh = (unsigned int)rhs;
      long norm_lhs = normalize_const_by_type(in->type, lhs);
      long folded = 0;
      if (in->op == MIR_OP_SHL) {
        folded = (long)((unsigned long)norm_lhs << sh);
      } else if (in->type && in->type->is_unsigned) {
        folded = (long)((unsigned long)norm_lhs >> sh);
      } else {
        folded = norm_lhs >> sh;
      }
      *out_kind = SCCP_LATTICE_CONST;
      *out_value = canonicalize_typed_imm(in->type, folded);
    } else if (lk == SCCP_LATTICE_OVERDEF || rk == SCCP_LATTICE_OVERDEF) {
      *out_kind = SCCP_LATTICE_OVERDEF;
    } else {
      *out_kind = SCCP_LATTICE_UNDEF;
    }
    return 1;
  }
  default:
    break;
  }

  switch (in->op) {
  case MIR_OP_NOP:
  case MIR_OP_LABEL:
  case MIR_OP_JMP:
  case MIR_OP_JZ:
  case MIR_OP_JCC:
  case MIR_OP_RET:
  case MIR_OP_STORE:
  case MIR_OP_STORE_LOCAL:
  case MIR_OP_MEMCPY:
    return 0;
  default:
    // メモリアクセス/呼び出し等は定数として扱わない
    *out_kind = SCCP_LATTICE_OVERDEF;
    return 1;
  }
}

static int mark_block_executable(unsigned char *exec, int block_len, int bi) {
  if (!exec || bi < 0 || bi >= block_len || exec[bi])
    return 0;
  exec[bi] = 1;
  return 1;
}

static void rewrite_inst_to_imm(MirInst *in, long imm) {
  if (!in)
    return;
  in->op = MIR_OP_IMM;
  in->src1 = MIR_INVALID_VREG;
  in->src2 = MIR_INVALID_VREG;
  in->imm = imm;
  in->label = MIR_INVALID_LABEL;
  in->offset = 0;
  in->var = NULL;
  in->call_fn = NULL;
  in->argc = 0;
  for (int a = 0; a < MAX_FUNC_PARAMS; a++)
    in->args[a] = MIR_INVALID_VREG;
}

void run_cleanup_sccp(MirFunction *mf) {
  if (!mf || mf->blocks[0].inst_len <= 0 || mf->next_vreg <= 0)
    return;

  CleanupBlock *blocks = NULL;
  int block_len = 0;
  build_cleanup_cfg(mf, &blocks, &block_len);
  if (!blocks || block_len <= 0) {
    free(blocks);
    return;
  }

  SccpLatticeKind *kinds = calloc(mf->next_vreg, sizeof(SccpLatticeKind));
  long *values = calloc(mf->next_vreg, sizeof(long));
  unsigned char *exec = calloc(block_len, sizeof(unsigned char));
  int *label_to_block = NULL;
  if (mf->next_label > 0) {
    label_to_block = malloc(sizeof(int) * mf->next_label);
    if (!label_to_block)
      error("memory allocation failed [in cleanup sccp label map]");
    for (int l = 0; l < mf->next_label; l++)
      label_to_block[l] = -1;
    for (int bi = 0; bi < block_len; bi++) {
      int first = blocks[bi].start;
      if (first < 0 || first >= mf->blocks[0].inst_len || mf->blocks[0].insts[first].op != MIR_OP_LABEL)
        continue;
      int l = mf->blocks[0].insts[first].label;
      if (l >= 0 && l < mf->next_label)
        label_to_block[l] = bi;
    }
  }
  if (!kinds || !values || !exec)
    error("memory allocation failed [in cleanup sccp setup]");

  if (block_len > 0)
    exec[0] = 1;

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int bi = 0; bi < block_len; bi++) {
      if (!exec[bi])
        continue;

      CleanupBlock *bb = &blocks[bi];
      for (int i = bb->start; i < bb->end; i++) {
        MirInst *in = &mf->blocks[0].insts[i];
        SccpLatticeKind nk = SCCP_LATTICE_UNDEF;
        long nv = 0;
        if (!sccp_eval_inst_result(in, mf->next_vreg, kinds, values, &nk, &nv))
          continue;
        if (in->dst < 0 || in->dst >= mf->next_vreg)
          continue;
        if (sccp_merge_lattice(&kinds[in->dst], &values[in->dst], nk, nv))
          changed = 1;
      }

      if (bb->start >= bb->end)
        continue;

      MirInst *term = &mf->blocks[0].insts[bb->end - 1];
      if (term->op == MIR_OP_RET)
        continue;

      if (term->op == MIR_OP_JMP) {
        int target = -1;
        if (label_to_block && term->label >= 0 && term->label < mf->next_label)
          target = label_to_block[term->label];
        if (mark_block_executable(exec, block_len, target))
          changed = 1;
        continue;
      }

      if (term->op == MIR_OP_JZ) {
        int target = -1;
        if (label_to_block && term->label >= 0 && term->label < mf->next_label)
          target = label_to_block[term->label];
        int fallthrough = (bi + 1 < block_len) ? (bi + 1) : -1;
        long cond = 0;
        SccpLatticeKind ck = sccp_vreg_kind(term->src1, mf->next_vreg, kinds, values, &cond);
        if (ck == SCCP_LATTICE_CONST) {
          int only = (cond == 0) ? target : fallthrough;
          if (mark_block_executable(exec, block_len, only))
            changed = 1;
        } else {
          if (mark_block_executable(exec, block_len, target))
            changed = 1;
          if (mark_block_executable(exec, block_len, fallthrough))
            changed = 1;
        }
        continue;
      }

      if (term->op == MIR_OP_JCC) {
        int target = -1;
        if (label_to_block && term->label >= 0 && term->label < mf->next_label)
          target = label_to_block[term->label];
        int fallthrough = (bi + 1 < block_len) ? (bi + 1) : -1;
        long lhs = 0;
        long rhs = 0;
        SccpLatticeKind lk = sccp_vreg_kind(term->src1, mf->next_vreg, kinds, values, &lhs);
        SccpLatticeKind rk = sccp_vreg_kind(term->src2, mf->next_vreg, kinds, values, &rhs);
        if (lk == SCCP_LATTICE_CONST && rk == SCCP_LATTICE_CONST) {
          int taken = eval_const_jcc(term->imm, term->type, lhs, rhs);
          int only = taken ? target : fallthrough;
          if (mark_block_executable(exec, block_len, only))
            changed = 1;
        } else {
          if (mark_block_executable(exec, block_len, target))
            changed = 1;
          if (mark_block_executable(exec, block_len, fallthrough))
            changed = 1;
        }
        continue;
      }

      for (int si = 0; si < bb->succ_cnt; si++) {
        if (mark_block_executable(exec, block_len, bb->succ[si]))
          changed = 1;
      }
    }
  }

  for (int bi = 0; bi < block_len; bi++) {
    CleanupBlock *bb = &blocks[bi];
    if (!exec[bi]) {
      for (int i = bb->start; i < bb->end; i++)
        clear_inst_to_nop(&mf->blocks[0].insts[i]);
      continue;
    }

    for (int i = bb->start; i < bb->end; i++) {
      MirInst *in = &mf->blocks[0].insts[i];
      if (in->dst >= 0 && in->dst < mf->next_vreg && kinds[in->dst] == SCCP_LATTICE_CONST && in->op != MIR_OP_IMM) {
        rewrite_inst_to_imm(in, values[in->dst]);
        continue;
      }

      if (in->op == MIR_OP_JZ) {
        long cond = 0;
        if (sccp_vreg_kind(in->src1, mf->next_vreg, kinds, values, &cond) != SCCP_LATTICE_CONST)
          continue;
        if (cond == 0) {
          in->op = MIR_OP_JMP;
          in->src1 = MIR_INVALID_VREG;
          in->src2 = MIR_INVALID_VREG;
        } else {
          clear_inst_to_nop(in);
        }
        continue;
      }

      if (in->op == MIR_OP_JCC) {
        long lhs = 0;
        long rhs = 0;
        if (sccp_vreg_kind(in->src1, mf->next_vreg, kinds, values, &lhs) != SCCP_LATTICE_CONST ||
            sccp_vreg_kind(in->src2, mf->next_vreg, kinds, values, &rhs) != SCCP_LATTICE_CONST)
          continue;
        if (eval_const_jcc(in->imm, in->type, lhs, rhs)) {
          in->op = MIR_OP_JMP;
          in->src1 = MIR_INVALID_VREG;
          in->src2 = MIR_INVALID_VREG;
        } else {
          clear_inst_to_nop(in);
        }
      }
    }
  }

  run_cleanup_simplify_jumps_to_next_label(&mf->blocks[0].insts, &mf->blocks[0].inst_len);
  run_cleanup_remove_nops(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap);
  run_cleanup_prune_unreachable_blocks(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
  run_cleanup_prune_unreferenced_labels(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
 
  free(label_to_block);
  free(exec);
  free(values);
  free(kinds);
  free(blocks);
}
