#include "../codegen_internal.h"
#include "diagnostics.h"
#include "internal.h"
#include "platform.h"
#include "runtime.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
  INLINE_MAX_INST_O1 = 8,
  INLINE_MAX_COST_O1 = 30,
  INLINE_MAX_CALLSITES_O1 = 2,
};

static void init_inst(MirInst *inst, MirOp op) {
  if (!inst)
    error("invalid MirInst pointer [in init_inst]");
  memset(inst, 0, sizeof(*inst));
  inst->op = op;
  inst->dst = MIR_INVALID_VREG;
  inst->src1 = MIR_INVALID_VREG;
  inst->src2 = MIR_INVALID_VREG;
  inst->label = MIR_INVALID_LABEL;
  inst->var = NULL;
  inst->call_fn = NULL;
  inst->argc = 0;
  for (int i = 0; i < MAX_FUNC_PARAMS; i++)
    inst->args[i] = MIR_INVALID_VREG;
}

static int find_lowered_function_index(Function **fns, int fn_count, Function *target) {
  if (!fns || fn_count <= 0 || !target)
    return -1;
  for (int i = 0; i < fn_count; i++) {
    if (fns[i] == target)
      return i;
  }
  if (!target->name || target->len <= 0)
    return -1;
  for (int i = 0; i < fn_count; i++) {
    if (!fns[i] || !fns[i]->name)
      continue;
    if (fns[i]->len == target->len && !strncmp(fns[i]->name, target->name, target->len))
      return i;
  }
  return -1;
}

static int mir_local_max_offset(const MirFunction *mf) {
  if (!mf)
    return 0;
  int max_off = 0;
  for (int i = 0; i < mf->inst_len; i++) {
    MirOp op = mf->insts[i].op;
    if (op != MIR_OP_LOAD_LOCAL && op != MIR_OP_STORE_LOCAL && op != MIR_OP_ADDR_LOCAL)
      continue;
    if (mf->insts[i].offset > max_off)
      max_off = mf->insts[i].offset;
  }
  return max_off;
}

static int mir_has_call_inst(const MirFunction *mf) {
  if (!mf)
    return 0;
  for (int i = 0; i < mf->inst_len; i++) {
    if (mf->insts[i].op == MIR_OP_CALL)
      return 1;
  }
  return 0;
}

static int mir_inline_cost(const MirFunction *mf) {
  if (!mf)
    return 0;
  int cost = 0;
  for (int i = 0; i < mf->inst_len; i++) {
    MirOp op = mf->insts[i].op;
    switch (op) {
    case MIR_OP_NOP:
    case MIR_OP_LABEL:
      break;
    case MIR_OP_IMM:
    case MIR_OP_MOV:
    case MIR_OP_CAST:
      cost += 1;
      break;
    case MIR_OP_LOAD_LOCAL:
    case MIR_OP_STORE_LOCAL:
    case MIR_OP_ADDR_LOCAL:
    case MIR_OP_ADDR_SYMBOL:
    case MIR_OP_ADDR_FUNC:
    case MIR_OP_ADDR_STRING:
    case MIR_OP_ADDR_ARRAY:
    case MIR_OP_ADDR_STRUCT_LITERAL:
      cost += 2;
      break;
    case MIR_OP_LOAD:
    case MIR_OP_STORE:
      cost += 3;
      break;
    case MIR_OP_ADD:
    case MIR_OP_SUB:
    case MIR_OP_BITAND:
    case MIR_OP_BITOR:
    case MIR_OP_BITXOR:
    case MIR_OP_SHL:
    case MIR_OP_SHR:
    case MIR_OP_BITNOT:
    case MIR_OP_EQ:
    case MIR_OP_NE:
    case MIR_OP_LT:
    case MIR_OP_LE:
      cost += 2;
      break;
    case MIR_OP_MUL:
    case MIR_OP_SDIV:
    case MIR_OP_UDIV:
    case MIR_OP_SMOD:
    case MIR_OP_UMOD:
      cost += 4;
      break;
    case MIR_OP_MEMCPY:
      cost += 8;
      break;
    case MIR_OP_JMP:
    case MIR_OP_JZ:
    case MIR_OP_JCC:
      cost += 2;
      break;
    case MIR_OP_CALL:
      cost += 8;
      break;
    case MIR_OP_RET:
      cost += 1;
      break;
    default:
      cost += 3;
      break;
    }
  }
  return cost;
}

typedef struct {
  int call_count;
  int addr_taken;
} InlineSiteInfo;

static void build_inline_site_info(const MirFunction *mfs, Function **fns, int fn_count, InlineSiteInfo *info) {
  if (!mfs || !fns || fn_count <= 0 || !info)
    return;
  for (int i = 0; i < fn_count; i++) {
    info[i].call_count = 0;
    info[i].addr_taken = 0;
  }
  for (int i = 0; i < fn_count; i++) {
    const MirFunction *mf = &mfs[i];
    for (int k = 0; k < mf->inst_len; k++) {
      const MirInst *in = &mf->insts[k];
      if (in->op != MIR_OP_CALL && in->op != MIR_OP_ADDR_FUNC)
        continue;
      if (!in->call_fn)
        continue;
      int callee_idx = find_lowered_function_index(fns, fn_count, in->call_fn);
      if (callee_idx < 0)
        continue;
      if (in->op == MIR_OP_CALL)
        info[callee_idx].call_count++;
      else
        info[callee_idx].addr_taken++;
    }
  }
}

static void append_inst_inline(MirInst **out, int *out_len, int *out_cap, const MirInst *in) {
  if (!out || !out_len || !out_cap || !in)
    error("invalid inline append state");
  *out = safe_realloc_array(*out, sizeof(MirInst), *out_len + 1, out_cap);
  (*out)[*out_len] = *in;
  (*out_len)++;
}

static VReg remap_inline_vreg(const VReg *vmap, int vmap_len, VReg v) {
  if (v == MIR_INVALID_VREG)
    return v;
  if (!vmap || v < 0 || v >= vmap_len)
    error("invalid vreg remap in inliner");
  return vmap[v];
}

static int should_inline_callsite(const MirFunction *caller, const MirFunction *callee, const MirInst *call,
                                  int optimize_level, int caller_idx, int callee_idx, const unsigned char *reach,
                                  int fn_count, const InlineSiteInfo *site_info) {
  if (!caller || !callee || !call)
    return 0;
  if (optimize_level <= 0)
    return 0;
  if (call->op != MIR_OP_CALL || !call->call_fn || call->src1 != MIR_INVALID_VREG)
    return 0;
  if (!callee->fn || !callee->fn->is_defined || !callee->fn->type || callee->fn->type->ty != TY_FUNC)
    return 0;
  if (callee == caller)
    return 0;
  if (optimize_level == 1 && !callee->fn->is_inline)
    return 0;
  if (optimize_level == 1 && !callee->fn->is_static)
    return 0;
  if (callee->fn->type->is_variadic)
    return 0;
  if (call->argc != callee->param_count || call->argc < 0 || call->argc > MAX_FUNC_PARAMS)
    return 0;
  if (callee->inst_len <= 0 || callee->inst_len > INLINE_MAX_INST_O1)
    return 0;
  if (mir_inline_cost(callee) > INLINE_MAX_COST_O1)
    return 0;
  if (!site_info || site_info[callee_idx].call_count <= 0 ||
      site_info[callee_idx].call_count > INLINE_MAX_CALLSITES_O1 || site_info[callee_idx].addr_taken != 0)
    return 0;
  if (caller_idx < 0 || callee_idx < 0 || caller_idx >= fn_count || callee_idx >= fn_count)
    return 0;
  if (reach && reach[callee_idx * fn_count + caller_idx])
    return 0;
  return 1;
}

static void expand_inline_callsite(MirFunction *caller, const MirFunction *callee, const MirInst *call, MirInst **out,
                                   int *out_len, int *out_cap, int *caller_local_max) {
  if (!caller || !callee || !call || !out || !out_len || !out_cap || !caller_local_max)
    error("invalid inline expansion state");

  int local_max = *caller_local_max;
  int offset_delta = ((local_max + 7) / 8) * 8;
  int callee_local_max = mir_local_max_offset(callee);
  if (offset_delta + callee_local_max > *caller_local_max)
    *caller_local_max = offset_delta + callee_local_max;

  VReg *vmap = NULL;
  if (callee->next_vreg > 0) {
    vmap = malloc(sizeof(VReg) * callee->next_vreg);
    if (!vmap)
      error("memory allocation failed [in inline vreg remap]");
    for (int v = 0; v < callee->next_vreg; v++)
      vmap[v] = mir_new_vreg(caller);
  }

  int *label_map = NULL;
  if (callee->next_label > 0) {
    label_map = malloc(sizeof(int) * callee->next_label);
    if (!label_map)
      error("memory allocation failed [in inline label remap]");
    for (int l = 0; l < callee->next_label; l++)
      label_map[l] = mir_new_label(caller);
  }
  int inline_end_label = mir_new_label(caller);

  for (int i = 0; i < callee->param_count; i++) {
    MirInst st;
    init_inst(&st, MIR_OP_STORE_LOCAL);
    st.offset = callee->param_offsets[i] + offset_delta;
    st.type = callee->param_types[i];
    st.src1 = call->args[i];
    append_inst_inline(out, out_len, out_cap, &st);
  }

  for (int i = 0; i < callee->inst_len; i++) {
    const MirInst *cin = &callee->insts[i];
    if (cin->op == MIR_OP_RET) {
      if (call->dst != MIR_INVALID_VREG && cin->src1 != MIR_INVALID_VREG) {
        MirInst mv;
        init_inst(&mv, MIR_OP_MOV);
        mv.dst = call->dst;
        mv.src1 = remap_inline_vreg(vmap, callee->next_vreg, cin->src1);
        mv.type = call->type;
        append_inst_inline(out, out_len, out_cap, &mv);
      }
      MirInst j;
      init_inst(&j, MIR_OP_JMP);
      j.label = inline_end_label;
      append_inst_inline(out, out_len, out_cap, &j);
      continue;
    }

    MirInst out_in = *cin;
    out_in.dst = remap_inline_vreg(vmap, callee->next_vreg, out_in.dst);
    out_in.src1 = remap_inline_vreg(vmap, callee->next_vreg, out_in.src1);
    out_in.src2 = remap_inline_vreg(vmap, callee->next_vreg, out_in.src2);
    for (int a = 0; a < out_in.argc; a++)
      out_in.args[a] = remap_inline_vreg(vmap, callee->next_vreg, out_in.args[a]);

    if (out_in.op == MIR_OP_LOAD_LOCAL || out_in.op == MIR_OP_STORE_LOCAL || out_in.op == MIR_OP_ADDR_LOCAL)
      out_in.offset += offset_delta;

    if (out_in.op == MIR_OP_LABEL || out_in.op == MIR_OP_JMP || out_in.op == MIR_OP_JZ || out_in.op == MIR_OP_JCC) {
      if (!label_map || out_in.label < 0 || out_in.label >= callee->next_label)
        error("invalid inline label remap");
      out_in.label = label_map[out_in.label];
    }

    append_inst_inline(out, out_len, out_cap, &out_in);
  }

  MirInst end_label;
  init_inst(&end_label, MIR_OP_LABEL);
  end_label.label = inline_end_label;
  append_inst_inline(out, out_len, out_cap, &end_label);

  free(label_map);
  free(vmap);
}

static int run_inline_in_function(MirFunction *caller, MirFunction *mfs, Function **fns, int fn_count,
                                  int optimize_level, int caller_idx, const unsigned char *reach,
                                  const InlineSiteInfo *site_info) {
  if (!caller || !mfs || !fns || fn_count <= 0)
    return 0;

  int changed = 0;
  int caller_local_max = mir_local_max_offset(caller);
  MirInst *out = NULL;
  int out_len = 0;
  int out_cap = 0;

  for (int i = 0; i < caller->inst_len; i++) {
    MirInst *in = &caller->insts[i];
    if (in->op == MIR_OP_CALL && in->call_fn) {
      int callee_idx = find_lowered_function_index(fns, fn_count, in->call_fn);
      if (callee_idx >= 0 && should_inline_callsite(caller, &mfs[callee_idx], in, optimize_level, caller_idx,
                                                    callee_idx, reach, fn_count, site_info)) {
        expand_inline_callsite(caller, &mfs[callee_idx], in, &out, &out_len, &out_cap, &caller_local_max);
        changed = 1;
        continue;
      }
    }
    append_inst_inline(&out, &out_len, &out_cap, in);
  }

  if (!changed) {
    free(out);
    return 0;
  }

  free(caller->insts);
  caller->insts = out;
  caller->inst_len = out_len;
  caller->inst_cap = out_cap;
  return 1;
}

static void build_call_reachability(const MirFunction *mfs, Function **fns, int fn_count, unsigned char *reach) {
  if (!mfs || !fns || fn_count <= 0 || !reach)
    return;
  int *queue = malloc(sizeof(int) * fn_count);
  unsigned char *visited = calloc(fn_count, sizeof(unsigned char));
  if (!queue || !visited)
    error("memory allocation failed [in build_call_reachability]");

  for (int i = 0; i < fn_count; i++) {
    for (int j = 0; j < fn_count; j++)
      reach[i * fn_count + j] = 0;
    for (int j = 0; j < fn_count; j++)
      visited[j] = 0;

    int qh = 0;
    int qt = 0;
    queue[qt++] = i;
    visited[i] = 1;

    while (qh < qt) {
      int from = queue[qh++];
      const MirFunction *mf = &mfs[from];
      for (int k = 0; k < mf->inst_len; k++) {
        const MirInst *in = &mf->insts[k];
        if (in->op != MIR_OP_CALL || !in->call_fn)
          continue;
        int to = find_lowered_function_index(fns, fn_count, in->call_fn);
        if (to < 0)
          continue;
        reach[i * fn_count + to] = 1;
        if (!visited[to]) {
          visited[to] = 1;
          queue[qt++] = to;
        }
      }
    }
  }

  free(visited);
  free(queue);
}

static void run_mir_inline_pass(MirFunction *mfs, Function **fns, int fn_count, int optimize_level) {
  if (!mfs || !fns || fn_count <= 0 || optimize_level <= 0)
    return;
  InlineSiteInfo *site_info = calloc(fn_count, sizeof(InlineSiteInfo));
  unsigned char *reach = calloc((size_t)fn_count * (size_t)fn_count, sizeof(unsigned char));
  if (!reach || !site_info)
    error("memory allocation failed [in run_mir_inline_pass]");
  build_inline_site_info(mfs, fns, fn_count, site_info);
  build_call_reachability(mfs, fns, fn_count, reach);
  for (int i = 0; i < fn_count; i++)
    run_inline_in_function(&mfs[i], mfs, fns, fn_count, optimize_level, i, reach, site_info);
  free(site_info);
  free(reach);
}

void emit_mir_program_pipeline(int dump_mir, int optimize_level) {
  int fn_count = 0;
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    if (code[i]->kind == ND_FUNCDEF)
      fn_count++;
  }
  if (fn_count <= 0)
    return;

#if LACC_PLATFORM_APPLE
  write_file("  .section __TEXT,__text,regular,pure_instructions\n");
#else
  write_file("  .text\n");
#endif

  MirFunction *mfs = calloc(fn_count, sizeof(MirFunction));
  Function **fns = calloc(fn_count, sizeof(Function *));
  unsigned char *reachable = calloc(fn_count, sizeof(unsigned char));
  int *queue = malloc(sizeof(int) * fn_count);
  if (!mfs || !fns || !reachable || !queue)
    error("memory allocation failed [in emit_mir_program reachability setup]");

  int idx = 0;
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    if (code[i]->kind != ND_FUNCDEF)
      continue;
    lower_function(code[i], &mfs[idx]);
    fns[idx] = mfs[idx].fn;
    idx++;
  }

  run_mir_inline_pass(mfs, fns, fn_count, optimize_level);
  if (optimize_level > 0) {
    for (int i = 0; i < fn_count; i++) {
      optimize_mir_inline_cleanup(&mfs[i]);
      optimize_mir_mem2reg(&mfs[i]);
    }
  }

  int qh = 0;
  int qt = 0;
  for (int i = 0; i < fn_count; i++) {
    if (!fns[i] || fns[i]->is_static)
      continue;
    if (optimize_level > 0 && fns[i]->is_inline)
      continue;
    reachable[i] = 1;
    queue[qt++] = i;
  }

  while (qh < qt) {
    int from = queue[qh++];
    MirFunction *mf = &mfs[from];
    for (int i = 0; i < mf->inst_len; i++) {
      MirInst *in = &mf->insts[i];
      if ((in->op != MIR_OP_CALL && in->op != MIR_OP_ADDR_FUNC) || !in->call_fn)
        continue;

      int to = find_lowered_function_index(fns, fn_count, in->call_fn);
      if (to < 0 || reachable[to])
        continue;
      reachable[to] = 1;
      queue[qt++] = to;
    }
  }

  for (int i = 0; i < fn_count; i++) {
    if (reachable[i]) {
      if (dump_mir)
        mir_dump(stderr, &mfs[i]);
      emit_mir_function_internal(&mfs[i]);
    }
    mir_free(&mfs[i]);
  }

  free(queue);
  free(reachable);
  free(fns);
  free(mfs);
}
