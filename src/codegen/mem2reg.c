#include "mem2reg.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  int offset;
  Type *type;
  int promotable;
} LocalSlot;

typedef struct {
  int valid;
  int dirty;
  VReg value;
} LocalState;

static int is_scalar_local_type(Type *type) {
  if (!type)
    return 0;
  switch (type->ty) {
  case TY_BOOL:
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
    return 1;
  default:
    return 0;
  }
}

static int local_access_compatible(Type *slot_type, Type *access_type) {
  if (!slot_type || !access_type)
    return 0;
  if (!is_scalar_local_type(slot_type) || !is_scalar_local_type(access_type))
    return 0;
  if (slot_type->ty != access_type->ty)
    return 0;
  if (slot_type->is_unsigned != access_type->is_unsigned)
    return 0;
  return 1;
}

static int find_slot(LocalSlot *slots, int slot_len, int offset) {
  for (int i = 0; i < slot_len; i++) {
    if (slots[i].offset == offset)
      return i;
  }
  return -1;
}

static int get_or_add_slot(LocalSlot *slots, int *slot_len, int slot_cap, int offset, Type *type) {
  int idx = find_slot(slots, *slot_len, offset);
  if (idx >= 0) {
    if (!local_access_compatible(slots[idx].type, type))
      slots[idx].promotable = 0;
    return idx;
  }
  if (*slot_len >= slot_cap)
    error("mem2reg slot capacity exhausted");
  idx = (*slot_len)++;
  slots[idx].offset = offset;
  slots[idx].type = type;
  slots[idx].promotable = is_scalar_local_type(type);
  return idx;
}

static void ensure_out_capacity(MirInst **out_insts, int *out_cap, int need) {
  if (*out_cap >= need)
    return;
  int new_cap = *out_cap ? *out_cap : 32;
  while (new_cap < need)
    new_cap *= 2;
  MirInst *new_buf = realloc(*out_insts, sizeof(MirInst) * new_cap);
  if (!new_buf)
    error("memory allocation failed [in ensure_out_capacity mem2reg]");
  *out_insts = new_buf;
  *out_cap = new_cap;
}

static void append_inst(MirInst **out_insts, int *out_len, int *out_cap, const MirInst *in) {
  ensure_out_capacity(out_insts, out_cap, *out_len + 1);
  (*out_insts)[(*out_len)++] = *in;
}

static void append_store_local(MirInst **out_insts, int *out_len, int *out_cap, int offset, Type *type, VReg value) {
  MirInst st;
  memset(&st, 0, sizeof(st));
  st.op = MIR_OP_STORE_LOCAL;
  st.dst = MIR_INVALID_VREG;
  st.src1 = value;
  st.src2 = MIR_INVALID_VREG;
  st.label = MIR_INVALID_LABEL;
  st.offset = offset;
  st.type = type;
  st.argc = 0;
  for (int i = 0; i < MAX_FUNC_PARAMS; i++)
    st.args[i] = MIR_INVALID_VREG;
  append_inst(out_insts, out_len, out_cap, &st);
}

static void append_cast(MirInst **out_insts, int *out_len, int *out_cap, VReg dst, VReg src, Type *type) {
  MirInst cs;
  memset(&cs, 0, sizeof(cs));
  cs.op = MIR_OP_CAST;
  cs.dst = dst;
  cs.src1 = src;
  cs.src2 = MIR_INVALID_VREG;
  cs.label = MIR_INVALID_LABEL;
  cs.type = type;
  cs.argc = 0;
  for (int i = 0; i < MAX_FUNC_PARAMS; i++)
    cs.args[i] = MIR_INVALID_VREG;
  append_inst(out_insts, out_len, out_cap, &cs);
}

static void flush_dirty_locals(MirInst **out_insts, int *out_len, int *out_cap, LocalSlot *slots, int slot_len,
                               LocalState *state, const int *remaining_reads, int allow_dead_store_elim) {
  for (int i = 0; i < slot_len; i++) {
    if (!slots[i].promotable || !state[i].dirty)
      continue;
    if (allow_dead_store_elim && remaining_reads && remaining_reads[i] <= 0) {
      // No future read can observe this local value; skip dead store.
      state[i].dirty = 0;
      continue;
    }
    append_store_local(out_insts, out_len, out_cap, slots[i].offset, slots[i].type, state[i].value);
    state[i].dirty = 0;
  }
}

static void invalidate_all_states(LocalState *state, int slot_len) {
  for (int i = 0; i < slot_len; i++) {
    state[i].valid = 0;
    state[i].dirty = 0;
    state[i].value = MIR_INVALID_VREG;
  }
}

static int is_control_barrier(MirOp op) {
  return op == MIR_OP_LABEL || op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET;
}

static int local_read_slot_index(const MirInst *in, LocalSlot *slots, int slot_len, const int *addr_slot_of_vreg,
                                 int vreg_count) {
  if (!in || !slots || slot_len <= 0)
    return -1;
  if (in->op == MIR_OP_LOAD) {
    if (in->src1 < 0 || in->src1 >= vreg_count || !addr_slot_of_vreg)
      return -1;
    int s = addr_slot_of_vreg[in->src1];
    if (s < 0 || s >= slot_len)
      return -1;
    if (!slots[s].promotable || !local_access_compatible(slots[s].type, in->type))
      return -1;
    return s;
  }
  if (in->op == MIR_OP_LOAD_LOCAL) {
    int s = find_slot(slots, slot_len, in->offset);
    if (s < 0 || s >= slot_len)
      return -1;
    if (!slots[s].promotable || !local_access_compatible(slots[s].type, in->type))
      return -1;
    return s;
  }
  return -1;
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

  long l = lhs;
  long r = rhs;
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

static void mark_vreg_live(unsigned char *live, int vreg_count, VReg v) {
  if (!live || vreg_count <= 0 || v < 0 || v >= vreg_count)
    return;
  live[v] = 1;
}

static void remap_inst_vreg(VReg *v, const int *map, int old_vreg_count) {
  if (!v || !map || old_vreg_count <= 0 || *v == MIR_INVALID_VREG)
    return;
  if (*v < 0 || *v >= old_vreg_count)
    error("invalid vreg index [in mem2reg compact remap]");
  int nv = map[*v];
  if (nv < 0)
    error("missing vreg remap entry [in mem2reg compact remap]");
  *v = nv;
}

static void run_mem2reg_compact_vregs(MirFunction *mf) {
  if (!mf || mf->inst_len <= 0 || mf->next_vreg <= 0)
    return;

  int old_vreg_count = mf->next_vreg;
  unsigned char *live = calloc(old_vreg_count, sizeof(unsigned char));
  int *map = malloc(sizeof(int) * old_vreg_count);
  if (!live || !map)
    error("memory allocation failed [in mem2reg compact vregs]");
  for (int v = 0; v < old_vreg_count; v++)
    map[v] = -1;

  for (int i = 0; i < mf->inst_len; i++) {
    MirInst *in = &mf->insts[i];
    mark_vreg_live(live, old_vreg_count, in->dst);
    mark_vreg_live(live, old_vreg_count, in->src1);
    mark_vreg_live(live, old_vreg_count, in->src2);
    if (in->op == MIR_OP_CALL) {
      for (int a = 0; a < in->argc; a++)
        mark_vreg_live(live, old_vreg_count, in->args[a]);
    }
  }

  int new_vreg_count = 0;
  for (int v = 0; v < old_vreg_count; v++) {
    if (!live[v])
      continue;
    map[v] = new_vreg_count++;
  }

  if (new_vreg_count == old_vreg_count) {
    free(map);
    free(live);
    return;
  }

  for (int i = 0; i < mf->inst_len; i++) {
    MirInst *in = &mf->insts[i];
    remap_inst_vreg(&in->dst, map, old_vreg_count);
    remap_inst_vreg(&in->src1, map, old_vreg_count);
    remap_inst_vreg(&in->src2, map, old_vreg_count);
    if (in->op == MIR_OP_CALL) {
      for (int a = 0; a < in->argc; a++)
        remap_inst_vreg(&in->args[a], map, old_vreg_count);
    }
  }
  mf->next_vreg = new_vreg_count;

  free(map);
  free(live);
}

static void run_mem2reg_prune_unreferenced_labels(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_label <= 0)
    return;

  unsigned char *referenced = calloc(next_label, sizeof(unsigned char));
  if (!referenced)
    error("memory allocation failed [in mem2reg prune labels]");

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

static void run_mem2reg_fuse_compare_jz(MirInst **insts, int *inst_len) {
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

static void run_mem2reg_const_fold(MirInst **insts, int *inst_len, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *def_count = calloc(next_vreg, sizeof(int));
  MirOp *def_op = calloc(next_vreg, sizeof(MirOp));
  long *def_imm = calloc(next_vreg, sizeof(long));
  if (!def_count || !def_op || !def_imm)
    error("memory allocation failed [in mem2reg const fold def info]");

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
    if (in->dst < 0 || in->dst >= next_vreg)
      continue;

    long folded = 0;
    int can_fold = 0;
    if (in->src1 >= 0 && in->src1 < next_vreg && in->src2 >= 0 && in->src2 < next_vreg) {
      long lhs = 0;
      long rhs = 0;
      if (get_single_def_imm(in->src1, next_vreg, def_count, def_op, def_imm, &lhs) &&
          get_single_def_imm(in->src2, next_vreg, def_count, def_op, def_imm, &rhs)) {
        can_fold = fold_binary_const(in->op, in->type, lhs, rhs, &folded);
      }
    } else if (in->src1 >= 0 && in->src1 < next_vreg) {
      long src = 0;
      if (get_single_def_imm(in->src1, next_vreg, def_count, def_op, def_imm, &src))
        can_fold = fold_unary_const(in->op, in->type, src, &folded);
    }
    if (!can_fold)
      continue;

    in->op = MIR_OP_IMM;
    in->src1 = MIR_INVALID_VREG;
    in->src2 = MIR_INVALID_VREG;
    in->imm = folded;
    in->label = MIR_INVALID_LABEL;
    in->offset = 0;
    in->argc = 0;
    for (int a = 0; a < MAX_FUNC_PARAMS; a++)
      in->args[a] = MIR_INVALID_VREG;
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

static void run_mem2reg_cfg_const_fold_branches(MirInst **insts, int *inst_len, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *def_count = calloc(next_vreg, sizeof(int));
  MirOp *def_op = calloc(next_vreg, sizeof(MirOp));
  long *def_imm = calloc(next_vreg, sizeof(long));
  if (!def_count || !def_op || !def_imm)
    error("memory allocation failed [in mem2reg cfg const fold setup]");

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
    if (in->op == MIR_OP_JZ) {
      long cond = 0;
      if (!get_single_def_imm(in->src1, next_vreg, def_count, def_op, def_imm, &cond))
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
      if (!get_single_def_imm(in->src1, next_vreg, def_count, def_op, def_imm, &lhs) ||
          !get_single_def_imm(in->src2, next_vreg, def_count, def_op, def_imm, &rhs))
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

  free(def_imm);
  free(def_op);
  free(def_count);
}

static void run_mem2reg_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg) {
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

  int *use_count = calloc(next_vreg, sizeof(int));
  unsigned char *dead = calloc(*inst_len, sizeof(unsigned char));
  if (!use_count || !dead)
    error("memory allocation failed [in mem2reg dce setup]");

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
  *inst_cap = out;

  free(use_count);
  free(dead);
}

static void bit_set64(unsigned long long *bits, int bit) { bits[bit / 64] |= (1ULL << (bit & 63)); }

static int bit_test64(const unsigned long long *bits, int bit) { return (bits[bit / 64] & (1ULL << (bit & 63))) != 0; }

static void run_mem2reg_dead_store_local_cfg(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0)
    return;

  int n = *inst_len;
  int slot_cap = n;
  int *slot_offsets = malloc(sizeof(int) * (slot_cap > 0 ? slot_cap : 1));
  unsigned char *slot_escaped = calloc(slot_cap > 0 ? slot_cap : 1, sizeof(unsigned char));
  if (!slot_offsets || !slot_escaped)
    error("memory allocation failed [in mem2reg dead-store slots]");

  int slot_len = 0;
  for (int i = 0; i < n; i++) {
    MirInst *in = &(*insts)[i];
    if (in->op != MIR_OP_LOAD_LOCAL && in->op != MIR_OP_STORE_LOCAL && in->op != MIR_OP_ADDR_LOCAL)
      continue;
    int s = -1;
    for (int j = 0; j < slot_len; j++) {
      if (slot_offsets[j] == in->offset) {
        s = j;
        break;
      }
    }
    if (s < 0) {
      if (slot_len >= slot_cap)
        error("mem2reg dead-store slot capacity exhausted");
      s = slot_len++;
      slot_offsets[s] = in->offset;
      slot_escaped[s] = 0;
    }
    if (in->op == MIR_OP_ADDR_LOCAL)
      slot_escaped[s] = 1;
  }

  if (slot_len <= 0) {
    free(slot_escaped);
    free(slot_offsets);
    return;
  }

  int *slot_bit = malloc(sizeof(int) * slot_len);
  if (!slot_bit)
    error("memory allocation failed [in mem2reg dead-store slot bits]");
  int bit_count = 0;
  for (int i = 0; i < slot_len; i++) {
    if (slot_escaped[i]) {
      slot_bit[i] = -1;
    } else {
      slot_bit[i] = bit_count++;
    }
  }
  if (bit_count <= 0) {
    free(slot_bit);
    free(slot_escaped);
    free(slot_offsets);
    return;
  }

  int words = (bit_count + 63) / 64;
  int *label_to_inst = NULL;
  if (next_label > 0) {
    label_to_inst = malloc(sizeof(int) * next_label);
    if (!label_to_inst)
      error("memory allocation failed [in mem2reg dead-store labels]");
    for (int l = 0; l < next_label; l++)
      label_to_inst[l] = -1;
    for (int i = 0; i < n; i++) {
      if ((*insts)[i].op != MIR_OP_LABEL)
        continue;
      int l = (*insts)[i].label;
      if (l >= 0 && l < next_label)
        label_to_inst[l] = i;
    }
  }

  unsigned long long *live_in = calloc((size_t)n * (size_t)words, sizeof(unsigned long long));
  unsigned long long *live_out = calloc((size_t)n * (size_t)words, sizeof(unsigned long long));
  unsigned long long *new_out = calloc(words, sizeof(unsigned long long));
  unsigned long long *new_in = calloc(words, sizeof(unsigned long long));
  if (!live_in || !live_out || !new_out || !new_in)
    error("memory allocation failed [in mem2reg dead-store liveness]");

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int i = n - 1; i >= 0; i--) {
      MirInst *in = &(*insts)[i];
      unsigned long long *out_row = &live_out[(size_t)i * (size_t)words];
      unsigned long long *in_row = &live_in[(size_t)i * (size_t)words];
      for (int w = 0; w < words; w++) {
        new_out[w] = 0ULL;
        new_in[w] = 0ULL;
      }

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
        if (si < 0 || si >= n)
          continue;
        unsigned long long *succ_in = &live_in[(size_t)si * (size_t)words];
        for (int w = 0; w < words; w++)
          new_out[w] |= succ_in[w];
      }
      for (int w = 0; w < words; w++)
        new_in[w] = new_out[w];

      int slot_idx = -1;
      if (in->op == MIR_OP_LOAD_LOCAL || in->op == MIR_OP_STORE_LOCAL) {
        for (int j = 0; j < slot_len; j++) {
          if (slot_offsets[j] == in->offset) {
            slot_idx = j;
            break;
          }
        }
      }
      int bit = (slot_idx >= 0) ? slot_bit[slot_idx] : -1;
      if (bit >= 0) {
        if (in->op == MIR_OP_STORE_LOCAL) {
          new_in[bit / 64] &= ~(1ULL << (bit & 63));
        } else if (in->op == MIR_OP_LOAD_LOCAL) {
          bit_set64(new_in, bit);
        }
      }

      for (int w = 0; w < words; w++) {
        if (out_row[w] != new_out[w] || in_row[w] != new_in[w]) {
          changed = 1;
          out_row[w] = new_out[w];
          in_row[w] = new_in[w];
        }
      }
    }
  }

  unsigned char *dead = calloc(n, sizeof(unsigned char));
  if (!dead)
    error("memory allocation failed [in mem2reg dead-store marks]");
  for (int i = 0; i < n; i++) {
    MirInst *in = &(*insts)[i];
    if (in->op != MIR_OP_STORE_LOCAL)
      continue;
    int slot_idx = -1;
    for (int j = 0; j < slot_len; j++) {
      if (slot_offsets[j] == in->offset) {
        slot_idx = j;
        break;
      }
    }
    if (slot_idx < 0)
      continue;
    int bit = slot_bit[slot_idx];
    if (bit < 0)
      continue;
    unsigned long long *out_row = &live_out[(size_t)i * (size_t)words];
    if (!bit_test64(out_row, bit))
      dead[i] = 1;
  }

  int out = 0;
  for (int i = 0; i < n; i++) {
    if (dead[i])
      continue;
    if (out != i)
      (*insts)[out] = (*insts)[i];
    out++;
  }
  *inst_len = out;
  *inst_cap = out;

  free(dead);
  free(new_in);
  free(new_out);
  free(live_out);
  free(live_in);
  free(label_to_inst);
  free(slot_bit);
  free(slot_escaped);
  free(slot_offsets);
}

static void run_mem2reg_prune_unreachable_blocks(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0)
    return;

  int n = *inst_len;
  int *label_to_inst = NULL;
  if (next_label > 0) {
    label_to_inst = malloc(sizeof(int) * next_label);
    if (!label_to_inst)
      error("memory allocation failed [in mem2reg prune unreachable labels]");
    for (int l = 0; l < next_label; l++)
      label_to_inst[l] = -1;
    for (int i = 0; i < n; i++) {
      if ((*insts)[i].op != MIR_OP_LABEL)
        continue;
      int l = (*insts)[i].label;
      if (l >= 0 && l < next_label)
        label_to_inst[l] = i;
    }
  }

  unsigned char *visited = calloc(n, sizeof(unsigned char));
  int *queue = malloc(sizeof(int) * n);
  if (!visited || !queue)
    error("memory allocation failed [in mem2reg prune unreachable work]");
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
} Mem2regBlock;

static int is_cfg_terminator(MirOp op) { return op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET; }

static void add_cfg_succ(Mem2regBlock *bb, int succ) {
  if (!bb || succ < 0)
    return;
  for (int i = 0; i < bb->succ_cnt; i++) {
    if (bb->succ[i] == succ)
      return;
  }
  if (bb->succ_cnt < 2)
    bb->succ[bb->succ_cnt++] = succ;
}

static void build_mem2reg_cfg(const MirFunction *mf, Mem2regBlock **out_blocks, int *out_block_len) {
  if (!mf || !out_blocks || !out_block_len)
    error("invalid cfg builder args [in mem2reg cfg]");

  int n = mf->inst_len;
  unsigned char *is_block_start = calloc(n > 0 ? n : 1, sizeof(unsigned char));
  if (!is_block_start)
    error("memory allocation failed [in mem2reg cfg starts]");

  if (n > 0)
    is_block_start[0] = 1;
  for (int i = 1; i < n; i++) {
    if (mf->insts[i].op == MIR_OP_LABEL || is_cfg_terminator(mf->insts[i - 1].op))
      is_block_start[i] = 1;
  }

  int block_len = 0;
  for (int i = 0; i < n; i++) {
    if (is_block_start[i])
      block_len++;
  }
  if (block_len <= 0)
    block_len = 1;

  Mem2regBlock *blocks = calloc(block_len, sizeof(Mem2regBlock));
  int *label_to_block = malloc(sizeof(int) * (mf->next_label > 0 ? mf->next_label : 1));
  if (!blocks || !label_to_block)
    error("memory allocation failed [in mem2reg cfg alloc]");

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
    MirInst *first = &mf->insts[blocks[bi].start];
    if (first->op == MIR_OP_LABEL && first->label >= 0 && first->label < mf->next_label)
      label_to_block[first->label] = bi;
  }

  for (int bi = 0; bi < block_len; bi++) {
    Mem2regBlock *bb = &blocks[bi];
    if (bb->start >= bb->end)
      continue;
    MirInst *term = &mf->insts[bb->end - 1];
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

static void transfer_mem2reg_state_over_block(const MirFunction *mf, const Mem2regBlock *bb, LocalSlot *slots, int slot_len,
                                              const int *addr_slot_of_vreg, LocalState *state) {
  if (!mf || !bb || !slots || slot_len <= 0 || !addr_slot_of_vreg || !state)
    return;

  for (int i = bb->start; i < bb->end; i++) {
    const MirInst *in = &mf->insts[i];
    if ((in->op == MIR_OP_LOAD || in->op == MIR_OP_STORE) && in->src1 >= 0 && in->src1 < mf->next_vreg) {
      int s = addr_slot_of_vreg[in->src1];
      if (s >= 0 && s < slot_len && slots[s].promotable) {
        if (in->op == MIR_OP_LOAD) {
          if (!state[s].valid) {
            state[s].valid = 1;
            state[s].value = in->dst;
            state[s].dirty = 0;
          }
        } else {
          state[s].valid = 1;
          state[s].value = in->src2;
          state[s].dirty = 0;
        }
      }
      continue;
    }

    if (in->op == MIR_OP_LOAD_LOCAL) {
      int s = find_slot(slots, slot_len, in->offset);
      if (s >= 0 && slots[s].promotable && local_access_compatible(slots[s].type, in->type)) {
        if (!state[s].valid) {
          state[s].valid = 1;
          state[s].value = in->dst;
          state[s].dirty = 0;
        }
      }
      continue;
    }

    if (in->op == MIR_OP_STORE_LOCAL) {
      int s = find_slot(slots, slot_len, in->offset);
      if (s >= 0 && slots[s].promotable && local_access_compatible(slots[s].type, in->type)) {
        state[s].valid = 1;
        state[s].value = in->src1;
        state[s].dirty = 0;
      }
    }
  }
}

static void compute_mem2reg_cfg_in_state(const MirFunction *mf, LocalSlot *slots, int slot_len, const int *addr_slot_of_vreg,
                                         const Mem2regBlock *blocks, int block_len, unsigned char *block_reachable,
                                         unsigned char *in_valid, VReg *in_value) {
  if (!mf || !slots || slot_len <= 0 || !addr_slot_of_vreg || !blocks || block_len <= 0 || !block_reachable || !in_valid ||
      !in_value)
    return;

  LocalState *tmp_state = calloc(slot_len, sizeof(LocalState));
  if (!tmp_state)
    error("memory allocation failed [in mem2reg cfg dataflow tmp]");
  for (int s = 0; s < slot_len; s++)
    tmp_state[s].value = MIR_INVALID_VREG;

  for (int i = 0; i < block_len; i++) {
    block_reachable[i] = 0;
    for (int s = 0; s < slot_len; s++) {
      in_valid[i * slot_len + s] = 0;
      in_value[i * slot_len + s] = MIR_INVALID_VREG;
    }
  }
  block_reachable[0] = 1;

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int bi = 0; bi < block_len; bi++) {
      if (!block_reachable[bi])
        continue;

      for (int s = 0; s < slot_len; s++) {
        int idx = bi * slot_len + s;
        tmp_state[s].valid = in_valid[idx];
        tmp_state[s].dirty = 0;
        tmp_state[s].value = in_value[idx];
      }
      transfer_mem2reg_state_over_block(mf, &blocks[bi], slots, slot_len, addr_slot_of_vreg, tmp_state);

      for (int k = 0; k < blocks[bi].succ_cnt; k++) {
        int succ = blocks[bi].succ[k];
        if (succ < 0 || succ >= block_len)
          continue;
        if (!block_reachable[succ]) {
          block_reachable[succ] = 1;
          changed = 1;
          for (int s = 0; s < slot_len; s++) {
            int idx = succ * slot_len + s;
            in_valid[idx] = tmp_state[s].valid ? 1 : 0;
            in_value[idx] = tmp_state[s].valid ? tmp_state[s].value : MIR_INVALID_VREG;
          }
          continue;
        }

        for (int s = 0; s < slot_len; s++) {
          int idx = succ * slot_len + s;
          if (!in_valid[idx])
            continue;
          if (!tmp_state[s].valid || in_value[idx] != tmp_state[s].value) {
            in_valid[idx] = 0;
            in_value[idx] = MIR_INVALID_VREG;
            changed = 1;
          }
        }
      }
    }
  }

  free(tmp_state);
}

void optimize_mir_inline_cleanup(MirFunction *mf) {
  if (!mf || mf->inst_len <= 0 || mf->next_vreg <= 0)
    return;

  run_mem2reg_const_fold(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_fuse_compare_jz(&mf->insts, &mf->inst_len);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_dead_store_local_cfg(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_cfg_const_fold_branches(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_prune_unreachable_blocks(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_prune_unreferenced_labels(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
}

void optimize_mir_mem2reg(MirFunction *mf) {
  if (!mf || mf->inst_len <= 0 || mf->next_vreg <= 0)
    return;

  int slot_cap = mf->inst_len;
  LocalSlot *slots = calloc(slot_cap > 0 ? slot_cap : 1, sizeof(LocalSlot));
  int *addr_slot_of_vreg = malloc(sizeof(int) * mf->next_vreg);
  if (!slots || !addr_slot_of_vreg)
    error("memory allocation failed [in optimize_mir_mem2reg setup]");
  for (int v = 0; v < mf->next_vreg; v++)
    addr_slot_of_vreg[v] = -1;

  int slot_len = 0;
  for (int i = 0; i < mf->inst_len; i++) {
    MirInst *in = &mf->insts[i];
    if ((in->op == MIR_OP_ADDR_LOCAL || in->op == MIR_OP_LOAD_LOCAL || in->op == MIR_OP_STORE_LOCAL) &&
        in->offset > 0) {
      int s = get_or_add_slot(slots, &slot_len, slot_cap, in->offset, in->type);
      if (in->op == MIR_OP_ADDR_LOCAL && in->dst >= 0 && in->dst < mf->next_vreg)
        addr_slot_of_vreg[in->dst] = s;
    }
  }
  if (slot_len == 0) {
    run_mem2reg_compact_vregs(mf);
    free(addr_slot_of_vreg);
    free(slots);
    return;
  }

  for (int i = 0; i < mf->inst_len; i++) {
    MirInst *in = &mf->insts[i];
    if (in->op == MIR_OP_LOAD && in->src1 >= 0 && in->src1 < mf->next_vreg) {
      int s = addr_slot_of_vreg[in->src1];
      if (s >= 0 && (!slots[s].promotable || !local_access_compatible(slots[s].type, in->type)))
        slots[s].promotable = 0;
      continue;
    }
    if (in->op == MIR_OP_STORE && in->src1 >= 0 && in->src1 < mf->next_vreg) {
      int s = addr_slot_of_vreg[in->src1];
      if (s >= 0 && (!slots[s].promotable || !local_access_compatible(slots[s].type, in->type)))
        slots[s].promotable = 0;
      // STORE の値側にローカルアドレスが現れるとき
      // 例: p = &a; *q = p;
      if (in->src2 >= 0 && in->src2 < mf->next_vreg) {
        int rhs_slot = addr_slot_of_vreg[in->src2];
        if (rhs_slot >= 0)
          slots[rhs_slot].promotable = 0;
      }
      continue;
    }

    if (in->src1 >= 0 && in->src1 < mf->next_vreg) {
      int s = addr_slot_of_vreg[in->src1];
      if (s >= 0)
        slots[s].promotable = 0;
    }
    if (in->src2 >= 0 && in->src2 < mf->next_vreg) {
      int s = addr_slot_of_vreg[in->src2];
      if (s >= 0)
        slots[s].promotable = 0;
    }
    for (int a = 0; a < in->argc; a++) {
      VReg arg = in->args[a];
      if (arg < 0 || arg >= mf->next_vreg)
        continue;
      int s = addr_slot_of_vreg[arg];
      if (s >= 0)
        slots[s].promotable = 0;
    }
  }

  int any_promotable = 0;
  for (int i = 0; i < slot_len; i++) {
    if (slots[i].promotable) {
      any_promotable = 1;
      break;
    }
  }
  if (!any_promotable) {
    run_mem2reg_compact_vregs(mf);
    free(addr_slot_of_vreg);
    free(slots);
    return;
  }

  Mem2regBlock *blocks = NULL;
  int block_len = 0;
  build_mem2reg_cfg(mf, &blocks, &block_len);

  unsigned char *block_reachable = calloc(block_len > 0 ? block_len : 1, sizeof(unsigned char));
  unsigned char *cfg_in_valid = calloc((size_t)(block_len > 0 ? block_len : 1) * (size_t)slot_len, sizeof(unsigned char));
  VReg *cfg_in_value = malloc(sizeof(VReg) * (size_t)(block_len > 0 ? block_len : 1) * (size_t)slot_len);
  if (!block_reachable || !cfg_in_valid || !cfg_in_value)
    error("memory allocation failed [in optimize_mir_mem2reg cfg state]");
  compute_mem2reg_cfg_in_state(mf, slots, slot_len, addr_slot_of_vreg, blocks, block_len, block_reachable, cfg_in_valid,
                               cfg_in_value);

  LocalState *state = calloc(slot_len, sizeof(LocalState));
  int *remaining_reads = calloc(slot_len, sizeof(int));
  MirInst *out_insts = NULL;
  int out_len = 0;
  int out_cap = 0;
  if (!state || !remaining_reads)
    error("memory allocation failed [in optimize_mir_mem2reg state]");
  for (int i = 0; i < slot_len; i++)
    state[i].value = MIR_INVALID_VREG;
  for (int i = 0; i < mf->inst_len; i++) {
    int s = local_read_slot_index(&mf->insts[i], slots, slot_len, addr_slot_of_vreg, mf->next_vreg);
    if (s >= 0)
      remaining_reads[s]++;
  }

  for (int bi = 0; bi < block_len; bi++) {
    for (int s = 0; s < slot_len; s++) {
      int idx = bi * slot_len + s;
      state[s].valid = (block_reachable[bi] ? cfg_in_valid[idx] : 0);
      state[s].dirty = 0;
      state[s].value = (state[s].valid ? cfg_in_value[idx] : MIR_INVALID_VREG);
    }

    for (int i = blocks[bi].start; i < blocks[bi].end; i++) {
      MirInst in = mf->insts[i];
      int read_slot = local_read_slot_index(&in, slots, slot_len, addr_slot_of_vreg, mf->next_vreg);
      if (in.op == MIR_OP_JMP || in.op == MIR_OP_JZ || in.op == MIR_OP_JCC || in.op == MIR_OP_RET)
        flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state, remaining_reads, in.op == MIR_OP_RET);

      // 昇格できるなら ADDR_LOCAL は不要なので出力しない
      if (in.op == MIR_OP_ADDR_LOCAL && in.dst >= 0 && in.dst < mf->next_vreg) {
        int s = addr_slot_of_vreg[in.dst];
        if (s >= 0 && slots[s].promotable)
          goto post_inst;
      }

      // LOAD はレジスタからの MOV に、STORE は状態の更新にそれぞれ置き換える
      if ((in.op == MIR_OP_LOAD || in.op == MIR_OP_STORE) && in.src1 >= 0 && in.src1 < mf->next_vreg) {
        int s = addr_slot_of_vreg[in.src1];
        if (s >= 0 && slots[s].promotable) {
          if (in.op == MIR_OP_LOAD) {
            if (state[s].valid) {
              append_cast(&out_insts, &out_len, &out_cap, in.dst, state[s].value, in.type);
            } else {
              in.op = MIR_OP_LOAD_LOCAL;
              in.src1 = MIR_INVALID_VREG;
              in.src2 = MIR_INVALID_VREG;
              in.offset = slots[s].offset;
              append_inst(&out_insts, &out_len, &out_cap, &in);
              state[s].valid = 1;
              state[s].dirty = 0;
              state[s].value = in.dst;
            }
          } else {
            // MIR_OP_STORE
            state[s].valid = 1;
            state[s].dirty = 1;
            state[s].value = in.src2;
          }
          goto post_inst;
        }
      }

      // 既にキャッシュがあれば MOV に置換、なければそのまま出してキャッシュ化
      if (in.op == MIR_OP_LOAD_LOCAL) {
        int s = find_slot(slots, slot_len, in.offset);
        if (s >= 0 && slots[s].promotable && local_access_compatible(slots[s].type, in.type)) {
          if (state[s].valid) {
            append_cast(&out_insts, &out_len, &out_cap, in.dst, state[s].value, in.type);
          } else {
            append_inst(&out_insts, &out_len, &out_cap, &in);
            state[s].valid = 1;
            state[s].dirty = 0;
            state[s].value = in.dst;
          }
          goto post_inst;
        }
      }

      // 即ストアせず state を dirty 更新だけにする
      if (in.op == MIR_OP_STORE_LOCAL) {
        int s = find_slot(slots, slot_len, in.offset);
        if (s >= 0 && slots[s].promotable && local_access_compatible(slots[s].type, in.type)) {
          state[s].valid = 1;
          state[s].dirty = 1;
          state[s].value = in.src1;
          goto post_inst;
        }
      }

      append_inst(&out_insts, &out_len, &out_cap, &in);

    post_inst:
      if (read_slot >= 0 && read_slot < slot_len && remaining_reads[read_slot] > 0)
        remaining_reads[read_slot]--;
    }

    // fallthrough で LABEL ブロックへ入る境界では stack を同期しておく
    if (bi + 1 < block_len && mf->insts[blocks[bi + 1].start].op == MIR_OP_LABEL)
      flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state, remaining_reads, 0);
  }

  free(mf->insts);
  mf->insts = out_insts;
  mf->inst_len = out_len;
  mf->inst_cap = out_cap;
  run_mem2reg_prune_unreferenced_labels(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_fuse_compare_jz(&mf->insts, &mf->inst_len);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_const_fold(&mf->insts, &mf->inst_len, mf->next_vreg);
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_prune_unreferenced_labels(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_compact_vregs(mf);

  free(cfg_in_value);
  free(cfg_in_valid);
  free(block_reachable);
  free(blocks);
  free(state);
  free(remaining_reads);
  free(addr_slot_of_vreg);
  free(slots);
}
