#include "../bitset.h"
#include "diagnostics.h"
#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include "../cleanup/internal.h"

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
  if (!mf || mf->blocks[0].inst_len <= 0 || mf->next_vreg <= 0)
    return;

  int old_vreg_count = mf->next_vreg;
  unsigned char *live = calloc(old_vreg_count, sizeof(unsigned char));
  int *map = malloc(sizeof(int) * old_vreg_count);
  if (!live || !map)
    error("memory allocation failed [in mem2reg compact vregs]");
  for (int v = 0; v < old_vreg_count; v++)
    map[v] = -1;

  for (int i = 0; i < mf->blocks[0].inst_len; i++) {
    MirInst *in = &mf->blocks[0].insts[i];
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

  for (int i = 0; i < mf->blocks[0].inst_len; i++) {
    MirInst *in = &mf->blocks[0].insts[i];
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
static int *build_label_to_inst_map(const MirInst *insts, int inst_len, int next_label, const char *pass_name) {
  if (!insts || inst_len <= 0 || next_label <= 0)
    return NULL;

  int *label_to_inst = malloc(sizeof(int) * next_label);
  if (!label_to_inst)
    error("memory allocation failed [in %s labels]", pass_name ? pass_name : "mem2reg");

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
void run_mem2reg_dead_store_local_cfg(MirInst **insts, int *inst_len, int *inst_cap, int next_label) {
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
  int *label_to_inst = build_label_to_inst_map(*insts, n, next_label, "mem2reg dead-store");

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
          bit_set(new_in, bit);
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
    if (!bit_get(out_row, bit))
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

typedef struct {
  int start;
  int end;
  int succ[2];
  int succ_cnt;
} Mem2regBlock;

static int is_cfg_terminator(MirOp op) {
  return op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET;
}

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

  int n = mf->blocks[0].inst_len;
  unsigned char *is_block_start = calloc(n > 0 ? n : 1, sizeof(unsigned char));
  if (!is_block_start)
    error("memory allocation failed [in mem2reg cfg starts]");

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
    MirInst *first = &mf->blocks[0].insts[blocks[bi].start];
    if (first->op == MIR_OP_LABEL && first->label >= 0 && first->label < mf->next_label)
      label_to_block[first->label] = bi;
  }

  for (int bi = 0; bi < block_len; bi++) {
    Mem2regBlock *bb = &blocks[bi];
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

static void transfer_mem2reg_state_over_block(const MirFunction *mf, const Mem2regBlock *bb, LocalSlot *slots,
                                              int slot_len, const int *addr_slot_of_vreg, LocalState *state) {
  if (!mf || !bb || !slots || slot_len <= 0 || !addr_slot_of_vreg || !state)
    return;

  for (int i = bb->start; i < bb->end; i++) {
    const MirInst *in = &mf->blocks[0].insts[i];
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

static void compute_mem2reg_cfg_in_state(const MirFunction *mf, LocalSlot *slots, int slot_len,
                                         const int *addr_slot_of_vreg, const Mem2regBlock *blocks, int block_len,
                                         unsigned char *block_reachable, unsigned char *in_valid, VReg *in_value) {
  if (!mf || !slots || slot_len <= 0 || !addr_slot_of_vreg || !blocks || block_len <= 0 || !block_reachable ||
      !in_valid || !in_value)
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

void mem2reg_run_promote(MirFunction *mf) {
  int slot_cap = mf->blocks[0].inst_len;
  LocalSlot *slots = calloc(slot_cap > 0 ? slot_cap : 1, sizeof(LocalSlot));
  int *addr_slot_of_vreg = malloc(sizeof(int) * mf->next_vreg);
  if (!slots || !addr_slot_of_vreg)
    error("memory allocation failed [in optimize_mir_mem2reg setup]");
  for (int v = 0; v < mf->next_vreg; v++)
    addr_slot_of_vreg[v] = -1;

  int slot_len = 0;
  for (int i = 0; i < mf->blocks[0].inst_len; i++) {
    MirInst *in = &mf->blocks[0].insts[i];
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

  for (int i = 0; i < mf->blocks[0].inst_len; i++) {
    MirInst *in = &mf->blocks[0].insts[i];
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
  unsigned char *cfg_in_valid =
      calloc((size_t)(block_len > 0 ? block_len : 1) * (size_t)slot_len, sizeof(unsigned char));
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
  for (int i = 0; i < mf->blocks[0].inst_len; i++) {
    int s = local_read_slot_index(&mf->blocks[0].insts[i], slots, slot_len, addr_slot_of_vreg, mf->next_vreg);
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
      MirInst in = mf->blocks[0].insts[i];
      int read_slot = local_read_slot_index(&in, slots, slot_len, addr_slot_of_vreg, mf->next_vreg);
      if (in.op == MIR_OP_JMP || in.op == MIR_OP_JZ || in.op == MIR_OP_JCC || in.op == MIR_OP_RET)
        flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state, remaining_reads,
                           in.op == MIR_OP_RET);

      // 昇格できるなら ADDR_LOCAL は不要なので出力しない
      if (in.op == MIR_OP_ADDR_LOCAL && in.dst >= 0 && in.dst < mf->next_vreg) {
        int s = addr_slot_of_vreg[in.dst];
        if (s >= 0 && slots[s].promotable)
          goto post_inst;
      }

      // LOAD はレジスタからの MOV に，STORE は状態の更新にそれぞれ置き換える
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

      // 既にキャッシュがあれば MOV に置換，なければそのまま出してキャッシュ化
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
    if (bi + 1 < block_len && mf->blocks[0].insts[blocks[bi + 1].start].op == MIR_OP_LABEL)
      flush_dirty_locals(&out_insts, &out_len, &out_cap, slots, slot_len, state, remaining_reads, 0);
  }

  free(mf->blocks[0].insts);
  mf->blocks[0].insts = out_insts;
  mf->blocks[0].inst_len = out_len;
  mf->blocks[0].inst_cap = out_cap;
  for (int iter = 0; iter < 3; iter++) {
    run_cleanup_fuse_compare_jz(&mf->blocks[0].insts, &mf->blocks[0].inst_len);
    run_cleanup_sccp(mf);
    run_cleanup_copyprop_and_dce(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_vreg);
    run_cleanup_dce(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_vreg);
    run_mem2reg_dead_store_local_cfg(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
    run_cleanup_canonicalize_commutative_imm_rhs(&mf->blocks[0].insts, &mf->blocks[0].inst_len, mf->next_vreg);
    run_cleanup_prune_unreachable_blocks(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
    run_cleanup_prune_unreferenced_labels(&mf->blocks[0].insts, &mf->blocks[0].inst_len, &mf->blocks[0].inst_cap, mf->next_label);
  }
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
