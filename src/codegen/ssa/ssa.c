#include "../codegen_internal.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  VReg *data;
  int len;
  int cap;
} VRegStack;

typedef struct {
  MirFunction *mf;
  int old_vregs;
  int var_count;
  unsigned char *promotable;
  Type **var_types;
  unsigned char *dom;
  int *idom;
  unsigned char *frontier;
  int *children;
  int *child_count;
  VRegStack *stacks;
} SsaCtx;

static void insert_before_terminator(MirBasicBlock *bb, const MirInst *inst);

static void free_block(MirBasicBlock *bb) {
  for (int i = 0; i < bb->phi_count; i++) {
    free(bb->phis[i].incoming_block);
    free(bb->phis[i].incoming_value);
  }
  free(bb->phis);
  free(bb->insts);
  free(bb->preds);
}

static void prune_unreachable_blocks(MirFunction *mf) {
  int n = mf->block_count;
  unsigned char *reachable = calloc(n, sizeof(unsigned char));
  int *queue = malloc(sizeof(int) * n);
  if (!reachable || !queue)
    error("memory allocation failed [in SSA reachable blocks]");
  int head = 0;
  int tail = 0;
  reachable[0] = 1;
  queue[tail++] = 0;
  while (head < tail) {
    int b = queue[head++];
    for (int i = 0; i < mf->blocks[b].succ_count; i++) {
      int next = mf->blocks[b].succ[i];
      if (next >= 0 && next < n && !reachable[next]) {
        reachable[next] = 1;
        queue[tail++] = next;
      }
    }
  }
  int count = 0;
  for (int b = 0; b < n; b++)
    count += reachable[b] ? 1 : 0;
  if (count != n) {
    MirBasicBlock *blocks = calloc(count, sizeof(MirBasicBlock));
    if (!blocks)
      error("memory allocation failed [in SSA block pruning]");
    int out = 0;
    for (int b = 0; b < n; b++) {
      if (reachable[b]) {
        blocks[out] = mf->blocks[b];
        blocks[out].id = out;
        out++;
      } else {
        free_block(&mf->blocks[b]);
      }
    }
    free(mf->blocks);
    mf->blocks = blocks;
    mf->block_count = count;
    mf->block_cap = count;
    mf->current_block = count - 1;
    mir_finalize_cfg(mf);
  }
  free(queue);
  free(reachable);
}

static int is_scalar(Type *type) {
  if (!type)
    return 0;
  return type->ty == TY_BOOL || type->ty == TY_CHAR || type->ty == TY_SHORT || type->ty == TY_INT ||
         type->ty == TY_LONG || type->ty == TY_LONGLONG || type->ty == TY_PTR || type->ty == TY_ARGARR;
}

static int is_param_slot(const MirFunction *mf, int slot) {
  for (int i = 0; i < mf->param_count; i++) {
    if (mf->param_slots[i] == slot)
      return 1;
  }
  return 0;
}

static int local_index(const MirFunction *mf, const MirInst *in) {
  if (in->offset <= 0 || in->offset > mf->local_count)
    error("invalid local slot [in SSA]");
  return in->offset - 1;
}

static void stack_push(VRegStack *s, VReg v) {
  if (s->len >= s->cap) {
    int cap = s->cap ? s->cap * 2 : 4;
    VReg *data = realloc(s->data, sizeof(VReg) * cap);
    if (!data)
      error("memory allocation failed [in SSA stack]");
    s->data = data;
    s->cap = cap;
  }
  s->data[s->len++] = v;
}

static VReg stack_top(VRegStack *s) {
  return s && s->len > 0 ? s->data[s->len - 1] : MIR_INVALID_VREG;
}

static void ensure_phi_capacity(MirBasicBlock *bb) {
  if (bb->phi_count < bb->phi_cap)
    return;
  int cap = bb->phi_cap ? bb->phi_cap * 2 : 4;
  MirPhi *phis = realloc(bb->phis, sizeof(MirPhi) * cap);
  if (!phis)
    error("memory allocation failed [in SSA phi]");
  bb->phis = phis;
  bb->phi_cap = cap;
}

static int block_has_phi(const MirBasicBlock *bb, int var) {
  for (int i = 0; i < bb->phi_count; i++) {
    if (bb->phis[i].var == var)
      return 1;
  }
  return 0;
}

static void add_phi(MirBasicBlock *bb, int var, Type *type) {
  if (block_has_phi(bb, var))
    return;
  ensure_phi_capacity(bb);
  MirPhi *phi = &bb->phis[bb->phi_count++];
  memset(phi, 0, sizeof(*phi));
  phi->var = var;
  phi->dst = MIR_INVALID_VREG;
  phi->type = type;
}

static void collect_promotable_locals(SsaCtx *ctx) {
  MirFunction *mf = ctx->mf;
  ctx->promotable = calloc(mf->local_count > 0 ? mf->local_count : 1, sizeof(unsigned char));
  if (!ctx->promotable)
    error("memory allocation failed [in SSA locals]");
  for (int i = 0; i < mf->local_count; i++)
    ctx->promotable[i] = is_scalar(mf->local_types[i]) ? 1 : 0;
  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      if (in->op == MIR_OP_ADDR_LOCAL)
        ctx->promotable[local_index(mf, in)] = 0;
    }
  }
}

static void collect_var_types(SsaCtx *ctx) {
  MirFunction *mf = ctx->mf;
  ctx->var_types = calloc(ctx->var_count > 0 ? ctx->var_count : 1, sizeof(Type *));
  if (!ctx->var_types)
    error("memory allocation failed [in SSA variable types]");
  for (int i = 0; i < mf->local_count; i++)
    ctx->var_types[ctx->old_vregs + i] = mf->local_types[i];
  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      if (in->dst >= 0 && in->dst < ctx->old_vregs && !ctx->var_types[in->dst])
        ctx->var_types[in->dst] = in->type;
    }
  }
}

static void compute_dominators(SsaCtx *ctx) {
  MirFunction *mf = ctx->mf;
  int n = mf->block_count;
  ctx->dom = calloc((size_t)n * (size_t)n, sizeof(unsigned char));
  ctx->idom = malloc(sizeof(int) * n);
  ctx->frontier = calloc((size_t)n * (size_t)n, sizeof(unsigned char));
  if (!ctx->dom || !ctx->idom || !ctx->frontier)
    error("memory allocation failed [in SSA dominators]");

  ctx->dom[0] = 1;
  for (int b = 1; b < n; b++)
    for (int d = 0; d < n; d++)
      ctx->dom[b * n + d] = 1;

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int b = 1; b < n; b++) {
      MirBasicBlock *bb = &mf->blocks[b];
      for (int d = 0; d < n; d++) {
        int value = bb->pred_count > 0;
        for (int p = 0; p < bb->pred_count; p++)
          value = value && ctx->dom[bb->preds[p] * n + d];
        if (d == b)
          value = 1;
        if (ctx->dom[b * n + d] != value) {
          ctx->dom[b * n + d] = value;
          changed = 1;
        }
      }
    }
  }

  ctx->idom[0] = -1;
  for (int b = 1; b < n; b++) {
    ctx->idom[b] = -1;
    for (int d = 0; d < n; d++) {
      if (d == b || !ctx->dom[b * n + d])
        continue;
      int closest = 1;
      for (int e = 0; e < n; e++) {
        if (e == b || e == d || !ctx->dom[b * n + e])
          continue;
        if (ctx->dom[e * n + d]) {
          closest = 0;
          break;
        }
      }
      if (closest) {
        ctx->idom[b] = d;
        break;
      }
    }
  }

  for (int b = 0; b < n; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    if (bb->pred_count < 2)
      continue;
    for (int p = 0; p < bb->pred_count; p++) {
      int runner = bb->preds[p];
      while (runner >= 0 && runner != ctx->idom[b]) {
        ctx->frontier[runner * n + b] = 1;
        runner = ctx->idom[runner];
      }
    }
  }
}

static void insert_phis(SsaCtx *ctx) {
  MirFunction *mf = ctx->mf;
  int n = mf->block_count;
  unsigned char *defs = calloc((size_t)ctx->var_count * (size_t)n, sizeof(unsigned char));
  unsigned char *uses = calloc((size_t)ctx->var_count * (size_t)n, sizeof(unsigned char));
  unsigned char *live_in = calloc((size_t)ctx->var_count * (size_t)n, sizeof(unsigned char));
  unsigned char *live_out = calloc((size_t)ctx->var_count * (size_t)n, sizeof(unsigned char));
  int *def_count = calloc(ctx->var_count > 0 ? ctx->var_count : 1, sizeof(int));
  int *queue = malloc(sizeof(int) * n);
  unsigned char *queued = calloc(n, sizeof(unsigned char));
  if (!defs || !uses || !live_in || !live_out || !def_count || !queue || !queued)
    error("memory allocation failed [in SSA phi placement]");

  for (int l = 0; l < mf->local_count; l++) {
    if (ctx->promotable[l] && is_param_slot(mf, l + 1))
      defs[(ctx->old_vregs + l) * n] = 1;
  }
  for (int b = 0; b < n; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      VReg operands[2 + MAX_FUNC_PARAMS];
      int operand_count = 0;
      operands[operand_count++] = in->src1;
      operands[operand_count++] = in->src2;
      for (int a = 0; a < in->argc; a++)
        operands[operand_count++] = in->args[a];
      for (int a = 0; a < operand_count; a++) {
        int var = operands[a];
        if (var >= 0 && var < ctx->old_vregs && !defs[var * n + b])
          uses[var * n + b] = 1;
      }
      if (in->op == MIR_OP_LOAD_LOCAL && ctx->promotable[local_index(mf, in)]) {
        int var = ctx->old_vregs + in->offset - 1;
        if (!defs[var * n + b])
          uses[var * n + b] = 1;
      }
      if (in->dst >= 0 && in->dst < ctx->old_vregs)
        defs[in->dst * n + b] = 1;
      if (in->op == MIR_OP_STORE_LOCAL && ctx->promotable[local_index(mf, in)])
        defs[(ctx->old_vregs + in->offset - 1) * n + b] = 1;
    }
  }

  for (int var = 0; var < ctx->var_count; var++) {
    for (int b = 0; b < n; b++)
      def_count[var] += defs[var * n + b] ? 1 : 0;
  }

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int b = n - 1; b >= 0; b--) {
      for (int var = 0; var < ctx->var_count; var++) {
        int out = 0;
        for (int s = 0; s < mf->blocks[b].succ_count; s++)
          out = out || live_in[var * n + mf->blocks[b].succ[s]];
        int in = uses[var * n + b] || (out && !defs[var * n + b]);
        if (live_out[var * n + b] != out || live_in[var * n + b] != in) {
          live_out[var * n + b] = out;
          live_in[var * n + b] = in;
          changed = 1;
        }
      }
    }
  }

  for (int var = 0; var < ctx->var_count; var++) {
    if (var < ctx->old_vregs && def_count[var] <= 1)
      continue;
    memset(queued, 0, n);
    int head = 0;
    int tail = 0;
    for (int b = 0; b < n; b++) {
      if (defs[var * n + b]) {
        queue[tail++] = b;
        queued[b] = 1;
      }
    }
    while (head < tail) {
      int x = queue[head++];
      for (int y = 0; y < n; y++) {
        if (!ctx->frontier[x * n + y] || !live_in[var * n + y] || block_has_phi(&mf->blocks[y], var))
          continue;
        add_phi(&mf->blocks[y], var, ctx->var_types[var]);
        if (!defs[var * n + y] && !queued[y]) {
          queue[tail++] = y;
          queued[y] = 1;
        }
      }
    }
  }

  free(queued);
  free(queue);
  free(def_count);
  free(live_out);
  free(live_in);
  free(uses);
  free(defs);
}

static void remap_use(SsaCtx *ctx, VReg *v) {
  if (!v || *v < 0 || *v >= ctx->old_vregs)
    return;
  VReg top = stack_top(&ctx->stacks[*v]);
  if (top == MIR_INVALID_VREG)
    error("use before definition in SSA rename");
  *v = top;
}

static void phi_add_incoming(MirPhi *phi, int block, VReg value) {
  if (phi->incoming_count >= phi->incoming_cap) {
    int cap = phi->incoming_cap ? phi->incoming_cap * 2 : 4;
    int *blocks = realloc(phi->incoming_block, sizeof(int) * cap);
    VReg *values = realloc(phi->incoming_value, sizeof(VReg) * cap);
    if (!blocks || !values)
      error("memory allocation failed [in SSA phi incoming]");
    phi->incoming_block = blocks;
    phi->incoming_value = values;
    phi->incoming_cap = cap;
  }
  int i = phi->incoming_count++;
  phi->incoming_block[i] = block;
  phi->incoming_value[i] = value;
}

static void append_inst(MirInst **out, int *len, int *cap, const MirInst *in) {
  if (*len >= *cap) {
    int next = *cap ? *cap * 2 : 16;
    MirInst *insts = realloc(*out, sizeof(MirInst) * next);
    if (!insts)
      error("memory allocation failed [in SSA instructions]");
    *out = insts;
    *cap = next;
  }
  (*out)[(*len)++] = *in;
}

static void rename_block(SsaCtx *ctx, int b) {
  MirBasicBlock *bb = &ctx->mf->blocks[b];
  int *pushed = malloc(sizeof(int) * (ctx->var_count * 3 + bb->inst_len + bb->phi_count + 1));
  int pushed_len = 0;
  if (!pushed)
    error("memory allocation failed [in SSA rename]");

  if (b == 0) {
    for (int l = 0; l < ctx->mf->local_count; l++) {
      if (!ctx->promotable[l] || !is_param_slot(ctx->mf, l + 1))
        continue;
      int var = ctx->old_vregs + l;
      VReg initial = mir_new_vreg(ctx->mf);
      stack_push(&ctx->stacks[var], initial);
      pushed[pushed_len++] = var;
    }
  }

  for (int i = 0; i < bb->phi_count; i++) {
    MirPhi *phi = &bb->phis[i];
    phi->dst = mir_new_vreg(ctx->mf);
    stack_push(&ctx->stacks[phi->var], phi->dst);
    pushed[pushed_len++] = phi->var;
  }

  MirInst *out = NULL;
  int out_len = 0;
  int out_cap = 0;
  if (b == 0) {
    for (int l = 0; l < ctx->mf->local_count; l++) {
      if (!ctx->promotable[l] || !is_param_slot(ctx->mf, l + 1))
        continue;
      MirInst load;
      memset(&load, 0, sizeof(load));
      load.op = MIR_OP_LOAD_LOCAL;
      load.dst = stack_top(&ctx->stacks[ctx->old_vregs + l]);
      load.src1 = MIR_INVALID_VREG;
      load.src2 = MIR_INVALID_VREG;
      load.label = MIR_INVALID_LABEL;
      load.offset = l + 1;
      load.type = ctx->mf->local_types[l];
      for (int a = 0; a < MAX_FUNC_PARAMS; a++)
        load.args[a] = MIR_INVALID_VREG;
      append_inst(&out, &out_len, &out_cap, &load);
    }
  }

  for (int i = 0; i < bb->inst_len; i++) {
    MirInst in = bb->insts[i];
    int loaded_local_var = -1;
    remap_use(ctx, &in.src1);
    remap_use(ctx, &in.src2);
    for (int a = 0; a < in.argc; a++)
      remap_use(ctx, &in.args[a]);

    if (in.op == MIR_OP_STORE_LOCAL && ctx->promotable[local_index(ctx->mf, &in)]) {
      int var = ctx->old_vregs + in.offset - 1;
      stack_push(&ctx->stacks[var], in.src1);
      pushed[pushed_len++] = var;
      continue;
    }

    if (in.op == MIR_OP_LOAD_LOCAL && ctx->promotable[local_index(ctx->mf, &in)]) {
      int var = ctx->old_vregs + in.offset - 1;
      VReg value = stack_top(&ctx->stacks[var]);
      if (value != MIR_INVALID_VREG) {
        in.op = MIR_OP_MOV;
        in.src1 = value;
        in.src2 = MIR_INVALID_VREG;
        in.offset = 0;
      } else {
        loaded_local_var = var;
      }
    }

    if (in.dst >= 0 && in.dst < ctx->old_vregs) {
      int var = in.dst;
      in.dst = mir_new_vreg(ctx->mf);
      stack_push(&ctx->stacks[var], in.dst);
      pushed[pushed_len++] = var;
    }
    if (loaded_local_var >= 0) {
      stack_push(&ctx->stacks[loaded_local_var], in.dst);
      pushed[pushed_len++] = loaded_local_var;
    }
    append_inst(&out, &out_len, &out_cap, &in);
  }

  free(bb->insts);
  bb->insts = out;
  bb->inst_len = out_len;
  bb->inst_cap = out_cap;

  for (int s = 0; s < bb->succ_count; s++) {
    MirBasicBlock *succ = &ctx->mf->blocks[bb->succ[s]];
    for (int p = 0; p < succ->phi_count; p++) {
      MirPhi *phi = &succ->phis[p];
      VReg value = stack_top(&ctx->stacks[phi->var]);
      if (value == MIR_INVALID_VREG) {
        MirInst undef;
        memset(&undef, 0, sizeof(undef));
        undef.op = MIR_OP_IMM;
        undef.dst = mir_new_vreg(ctx->mf);
        undef.src1 = MIR_INVALID_VREG;
        undef.src2 = MIR_INVALID_VREG;
        undef.label = MIR_INVALID_LABEL;
        undef.type = phi->type;
        for (int a = 0; a < MAX_FUNC_PARAMS; a++)
          undef.args[a] = MIR_INVALID_VREG;
        insert_before_terminator(bb, &undef);
        value = undef.dst;
        stack_push(&ctx->stacks[phi->var], value);
        pushed[pushed_len++] = phi->var;
      }
      phi_add_incoming(phi, b, value);
    }
  }

  for (int i = 0; i < ctx->child_count[b]; i++)
    rename_block(ctx, ctx->children[b * ctx->mf->block_count + i]);

  for (int i = pushed_len - 1; i >= 0; i--)
    ctx->stacks[pushed[i]].len--;
  free(pushed);
}

static VReg resolve_alias(VReg *alias, VReg v) {
  while (v >= 0 && alias[v] != v) {
    alias[v] = alias[alias[v]];
    v = alias[v];
  }
  return v;
}

static int is_pure(MirOp op) {
  return op == MIR_OP_IMM || op == MIR_OP_MOV || op == MIR_OP_CAST || op == MIR_OP_ADD || op == MIR_OP_SUB ||
         op == MIR_OP_MUL || op == MIR_OP_SDIV || op == MIR_OP_UDIV || op == MIR_OP_SMOD || op == MIR_OP_UMOD ||
         op == MIR_OP_EQ || op == MIR_OP_NE || op == MIR_OP_LT || op == MIR_OP_LE || op == MIR_OP_BITAND ||
         op == MIR_OP_BITOR || op == MIR_OP_BITXOR || op == MIR_OP_SHL || op == MIR_OP_SHR ||
         op == MIR_OP_BITNOT || op == MIR_OP_ADDR_STRING || op == MIR_OP_ADDR_ARRAY ||
         op == MIR_OP_ADDR_STRUCT_LITERAL || op == MIR_OP_ADDR_SYMBOL || op == MIR_OP_ADDR_FUNC;
}

static void optimize_ssa_values(MirFunction *mf) {
  int n = mf->next_vreg;
  VReg *alias = malloc(sizeof(VReg) * n);
  int *uses = calloc(n, sizeof(int));
  if (!alias || !uses)
    error("memory allocation failed [in SSA optimization]");
  for (int v = 0; v < n; v++)
    alias[v] = v;

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int b = 0; b < mf->block_count; b++) {
      MirBasicBlock *bb = &mf->blocks[b];
      for (int p = 0; p < bb->phi_count; p++) {
        MirPhi *phi = &bb->phis[p];
        if (phi->incoming_count <= 0)
          continue;
        VReg same = resolve_alias(alias, phi->incoming_value[0]);
        for (int i = 1; i < phi->incoming_count; i++) {
          if (resolve_alias(alias, phi->incoming_value[i]) != same)
            same = MIR_INVALID_VREG;
        }
        if (same != MIR_INVALID_VREG && alias[phi->dst] != same) {
          alias[phi->dst] = same;
          changed = 1;
        }
      }
      for (int i = 0; i < bb->inst_len; i++) {
        MirInst *in = &bb->insts[i];
        if (in->op == MIR_OP_MOV && in->dst >= 0 && in->src1 >= 0) {
          VReg src = resolve_alias(alias, in->src1);
          if (alias[in->dst] != src) {
            alias[in->dst] = src;
            changed = 1;
          }
        }
      }
    }
  }

  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int p = 0; p < bb->phi_count; p++) {
      for (int i = 0; i < bb->phis[p].incoming_count; i++)
        bb->phis[p].incoming_value[i] = resolve_alias(alias, bb->phis[p].incoming_value[i]);
    }
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      if (in->src1 >= 0)
        in->src1 = resolve_alias(alias, in->src1);
      if (in->src2 >= 0)
        in->src2 = resolve_alias(alias, in->src2);
      for (int a = 0; a < in->argc; a++)
        in->args[a] = resolve_alias(alias, in->args[a]);
    }
  }

  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    int out = 0;
    for (int p = 0; p < bb->phi_count; p++) {
      MirPhi *phi = &bb->phis[p];
      if (alias[phi->dst] != phi->dst) {
        free(phi->incoming_block);
        free(phi->incoming_value);
        continue;
      }
      bb->phis[out++] = *phi;
    }
    bb->phi_count = out;
  }

  changed = 1;
  while (changed) {
    changed = 0;
    memset(uses, 0, sizeof(int) * n);
    for (int b = 0; b < mf->block_count; b++) {
      MirBasicBlock *bb = &mf->blocks[b];
      for (int p = 0; p < bb->phi_count; p++)
        for (int i = 0; i < bb->phis[p].incoming_count; i++)
          uses[bb->phis[p].incoming_value[i]]++;
      for (int i = 0; i < bb->inst_len; i++) {
        MirInst *in = &bb->insts[i];
        if (in->dst >= 0 && alias[in->dst] != in->dst && is_pure(in->op))
          continue;
        if (in->src1 >= 0)
          uses[in->src1]++;
        if (in->src2 >= 0)
          uses[in->src2]++;
        for (int a = 0; a < in->argc; a++)
          uses[in->args[a]]++;
      }
    }
    for (int b = 0; b < mf->block_count; b++) {
      MirBasicBlock *bb = &mf->blocks[b];
      int out = 0;
      for (int i = 0; i < bb->inst_len; i++) {
        MirInst *in = &bb->insts[i];
        int dead_alias = in->dst >= 0 && alias[in->dst] != in->dst && is_pure(in->op);
        int dead_value = in->dst >= 0 && uses[in->dst] == 0 && is_pure(in->op);
        if (dead_alias || dead_value) {
          changed = 1;
          continue;
        }
        bb->insts[out++] = *in;
      }
      bb->inst_len = out;
    }
  }

  free(uses);
  free(alias);
}

void optimize_mir_ssa(MirFunction *mf) {
  if (!mf)
    error("invalid MIR function [in optimize_mir_ssa]");
  if (mf->block_count <= 0 || mf->next_vreg <= 0)
    return;
  mir_finalize_cfg(mf);
  prune_unreachable_blocks(mf);
  SsaCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.mf = mf;
  ctx.old_vregs = mf->next_vreg;
  ctx.var_count = ctx.old_vregs + mf->local_count;
  collect_promotable_locals(&ctx);
  collect_var_types(&ctx);
  compute_dominators(&ctx);
  insert_phis(&ctx);

  int n = mf->block_count;
  ctx.children = malloc(sizeof(int) * n * n);
  ctx.child_count = calloc(n, sizeof(int));
  ctx.stacks = calloc(ctx.var_count > 0 ? ctx.var_count : 1, sizeof(VRegStack));
  if (!ctx.children || !ctx.child_count || !ctx.stacks)
    error("memory allocation failed [in SSA rename setup]");
  for (int b = 1; b < n; b++) {
    int parent = ctx.idom[b];
    if (parent >= 0)
      ctx.children[parent * n + ctx.child_count[parent]++] = b;
  }
  rename_block(&ctx, 0);
  optimize_ssa_values(mf);

  for (int v = 0; v < ctx.var_count; v++)
    free(ctx.stacks[v].data);
  free(ctx.stacks);
  free(ctx.child_count);
  free(ctx.children);
  free(ctx.frontier);
  free(ctx.idom);
  free(ctx.dom);
  free(ctx.var_types);
  free(ctx.promotable);
}

static void insert_before_terminator(MirBasicBlock *bb, const MirInst *inst) {
  int pos = bb->inst_len;
  if (pos > 0) {
    MirOp op = bb->insts[pos - 1].op;
    if (op == MIR_OP_JMP || op == MIR_OP_JZ || op == MIR_OP_JCC || op == MIR_OP_RET)
      pos--;
  }
  if (bb->inst_len >= bb->inst_cap) {
    int cap = bb->inst_cap ? bb->inst_cap * 2 : 8;
    MirInst *insts = realloc(bb->insts, sizeof(MirInst) * cap);
    if (!insts)
      error("memory allocation failed [in SSA destruction]");
    bb->insts = insts;
    bb->inst_cap = cap;
  }
  memmove(&bb->insts[pos + 1], &bb->insts[pos], sizeof(MirInst) * (bb->inst_len - pos));
  bb->insts[pos] = *inst;
  bb->inst_len++;
}

static void insert_copy(MirBasicBlock *bb, VReg dst, VReg src, Type *type) {
  if (dst == src)
    return;
  MirInst copy = {0};
  copy.op = MIR_OP_MOV;
  copy.dst = dst;
  copy.src1 = src;
  copy.src2 = MIR_INVALID_VREG;
  copy.label = MIR_INVALID_LABEL;
  copy.type = type;
  insert_before_terminator(bb, &copy);
}

static void insert_parallel_copies(MirFunction *mf, MirBasicBlock *bb, VReg *dst, VReg *src, Type **types,
                                   int count) {
  unsigned char *pending = calloc(count > 0 ? count : 1, sizeof(unsigned char));
  if (!pending)
    error("memory allocation failed [in SSA parallel copy]");
  int remaining = 0;
  for (int i = 0; i < count; i++) {
    if (dst[i] != src[i]) {
      pending[i] = 1;
      remaining++;
    }
  }

  while (remaining > 0) {
    int emitted = 0;
    for (int i = 0; i < count; i++) {
      if (!pending[i])
        continue;
      int dst_is_source = 0;
      for (int j = 0; j < count; j++)
        dst_is_source = dst_is_source || (pending[j] && src[j] == dst[i]);
      if (dst_is_source)
        continue;
      insert_copy(bb, dst[i], src[i], types[i]);
      pending[i] = 0;
      remaining--;
      emitted = 1;
    }
    if (emitted)
      continue;

    int cycle = 0;
    while (!pending[cycle])
      cycle++;
    VReg saved = mir_new_vreg(mf);
    insert_copy(bb, saved, dst[cycle], types[cycle]);
    for (int i = 0; i < count; i++) {
      if (pending[i] && src[i] == dst[cycle])
        src[i] = saved;
    }
  }
  free(pending);
}

void destruct_mir_ssa(MirFunction *mf) {
  if (!mf)
    error("invalid MIR function [in destruct_mir_ssa]");
  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    if (bb->phi_count == 0)
      continue;
    for (int pred_index = 0; pred_index < bb->pred_count; pred_index++) {
      int pred = bb->preds[pred_index];
      VReg *dst = malloc(sizeof(VReg) * bb->phi_count);
      VReg *src = malloc(sizeof(VReg) * bb->phi_count);
      Type **types = malloc(sizeof(Type *) * bb->phi_count);
      if (!dst || !src || !types)
        error("memory allocation failed [in SSA parallel copy]");
      for (int p = 0; p < bb->phi_count; p++) {
        MirPhi *phi = &bb->phis[p];
        dst[p] = phi->dst;
        src[p] = MIR_INVALID_VREG;
        types[p] = phi->type;
        for (int i = 0; i < phi->incoming_count; i++) {
          if (phi->incoming_block[i] == pred) {
            src[p] = phi->incoming_value[i];
            break;
          }
        }
        if (src[p] == MIR_INVALID_VREG)
          error("missing phi incoming value");
      }
      insert_parallel_copies(mf, &mf->blocks[pred], dst, src, types, bb->phi_count);
      free(types);
      free(src);
      free(dst);
    }
    for (int p = 0; p < bb->phi_count; p++) {
      free(bb->phis[p].incoming_block);
      free(bb->phis[p].incoming_value);
    }
    free(bb->phis);
    bb->phis = NULL;
    bb->phi_count = 0;
    bb->phi_cap = 0;
  }
}
