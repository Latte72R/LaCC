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
  return op == MIR_OP_MOV || op == MIR_OP_IMM || op == MIR_OP_CAST;
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

static void run_mem2reg_copyprop_and_dce(MirInst **insts, int *inst_len, int *inst_cap, int next_vreg) {
  if (!insts || !*insts || !inst_len || *inst_len <= 0 || next_vreg <= 0)
    return;

  int *def_count = calloc(next_vreg, sizeof(int));
  MirOp *def_op = calloc(next_vreg, sizeof(MirOp));
  Type **def_type = calloc(next_vreg, sizeof(Type *));
  if (!def_count || !def_op || !def_type)
    error("memory allocation failed [in mem2reg copy-prop def info]");
  for (int i = 0; i < *inst_len; i++) {
    MirInst *in = &(*insts)[i];
    if (in->dst < 0 || in->dst >= next_vreg)
      continue;
    def_count[in->dst]++;
    def_op[in->dst] = in->op;
    def_type[in->dst] = in->type;
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
        in->dst != in->src1 && def_count[in->src1] == 1 && same_scalar_type(def_type[in->src1], in->type) &&
        def_op[in->src1] != MIR_OP_NOP) {
      copy_src[in->dst] = in->src1;
    }

    if (is_control_barrier(in->op))
      clear_copy_relations(copy_src, next_vreg);
  }

  free(copy_src);
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

  for (int i = 0; i < mf->inst_len; i++) {
    MirInst in = mf->insts[i];
    int read_slot = local_read_slot_index(&in, slots, slot_len, addr_slot_of_vreg, mf->next_vreg);
    if (is_control_barrier(in.op))
      flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state, remaining_reads, in.op == MIR_OP_RET);

    // 昇格できるなら ADDR_LOCAL は不要なので出力しない
    if (in.op == MIR_OP_ADDR_LOCAL && in.dst >= 0 && in.dst < mf->next_vreg) {
      int s = addr_slot_of_vreg[in.dst];
      if (s >= 0 && slots[s].promotable)
        goto post_barrier;
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
        goto post_barrier;
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
        goto post_barrier;
      }
    }

    // 即ストアせず state を dirty 更新だけにする
    if (in.op == MIR_OP_STORE_LOCAL) {
      int s = find_slot(slots, slot_len, in.offset);
      if (s >= 0 && slots[s].promotable && local_access_compatible(slots[s].type, in.type)) {
        state[s].valid = 1;
        state[s].dirty = 1;
        state[s].value = in.src1;
        goto post_barrier;
      }
    }

    append_inst(&out_insts, &out_len, &out_cap, &in);

  post_barrier:
    if (in.op == MIR_OP_LABEL || in.op == MIR_OP_JMP || in.op == MIR_OP_RET)
      invalidate_all_states(state, slot_len);
    if (read_slot >= 0 && read_slot < slot_len && remaining_reads[read_slot] > 0)
      remaining_reads[read_slot]--;
  }

  free(mf->insts);
  mf->insts = out_insts;
  mf->inst_len = out_len;
  mf->inst_cap = out_cap;
  run_mem2reg_copyprop_and_dce(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_vreg);
  run_mem2reg_prune_unreferenced_labels(&mf->insts, &mf->inst_len, &mf->inst_cap, mf->next_label);
  run_mem2reg_compact_vregs(mf);

  free(state);
  free(remaining_reads);
  free(addr_slot_of_vreg);
  free(slots);
}
