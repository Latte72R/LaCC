#include "diagnostics.h"
#include "opt_internal.h"

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

static void append_mov(MirInst **out_insts, int *out_len, int *out_cap, VReg dst, VReg src, Type *type) {
  if (dst == src)
    return;
  MirInst mv;
  memset(&mv, 0, sizeof(mv));
  mv.op = MIR_OP_MOV;
  mv.dst = dst;
  mv.src1 = src;
  mv.src2 = MIR_INVALID_VREG;
  mv.label = MIR_INVALID_LABEL;
  mv.type = type;
  mv.argc = 0;
  for (int i = 0; i < MAX_FUNC_PARAMS; i++)
    mv.args[i] = MIR_INVALID_VREG;
  append_inst(out_insts, out_len, out_cap, &mv);
}

static void flush_dirty_locals(MirInst **out_insts, int *out_len, int *out_cap, LocalSlot *slots, int slot_len,
                               LocalState *state) {
  for (int i = 0; i < slot_len; i++) {
    if (!slots[i].promotable || !state[i].dirty)
      continue;
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
  return op == MIR_OP_LABEL || op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_RET;
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
    free(addr_slot_of_vreg);
    free(slots);
    return;
  }

  LocalState *state = calloc(slot_len, sizeof(LocalState));
  MirInst *out_insts = NULL;
  int out_len = 0;
  int out_cap = 0;
  if (!state)
    error("memory allocation failed [in optimize_mir_mem2reg state]");
  for (int i = 0; i < slot_len; i++)
    state[i].value = MIR_INVALID_VREG;

  for (int i = 0; i < mf->inst_len; i++) {
    MirInst in = mf->insts[i];
    if (is_control_barrier(in.op))
      flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state);

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
            append_mov(&out_insts, &out_len, &out_cap, in.dst, state[s].value, in.type);
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
          append_mov(&out_insts, &out_len, &out_cap, in.dst, state[s].value, in.type);
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
    if (in.op == MIR_OP_LABEL)
      invalidate_all_states(state, slot_len);
    if (in.op == MIR_OP_JMP || in.op == MIR_OP_RET)
      invalidate_all_states(state, slot_len);
  }

  free(mf->insts);
  mf->insts = out_insts;
  mf->inst_len = out_len;
  mf->inst_cap = out_cap;

  free(state);
  free(addr_slot_of_vreg);
  free(slots);
}
