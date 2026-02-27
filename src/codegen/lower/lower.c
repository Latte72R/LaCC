#include "diagnostics.h"
#include "runtime.h"
#include "../codegen_internal.h"

#include <stdlib.h>
#include <string.h>

enum {
  MAX_CTRL_DEPTH = 128,
  INLINE_MAX_INST_O1 = 8,
  INLINE_MAX_COST_O1 = 30,
  INLINE_MAX_CALLSITES_O1 = 2,
};

typedef struct ControlFrame ControlFrame;
struct ControlFrame {
  int break_label;
  int continue_label;
};

typedef struct AstLabelMap AstLabelMap;
struct AstLabelMap {
  AstLabelMap *next;
  int ast_label_id;
  int mir_label;
};

typedef struct SwitchCaseMap SwitchCaseMap;
struct SwitchCaseMap {
  SwitchCaseMap *next;
  int case_value;
  int case_label;
};

typedef struct SwitchCtx SwitchCtx;
struct SwitchCtx {
  SwitchCtx *next;
  int switch_id;
  int end_label;
  int default_label;
  SwitchCaseMap *cases;
};

// ASTをMIRへlowerするときに必要な情報
typedef struct LowerCtx LowerCtx;
struct LowerCtx {
  MirFunction *mf;
  Function *fn;
  int ctrl_depth;
  ControlFrame ctrl_stack[MAX_CTRL_DEPTH];
  AstLabelMap *label_map;
  SwitchCtx *switch_stack;
};

static void lower_error_node(const char *msg, Node *node) {
  if (node)
    error("%s [node kind=%d]", msg, node->kind);
  error("%s", msg);
}

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

static inline int new_label(LowerCtx *ctx) { return mir_new_label(ctx->mf); }

static void emit_label(LowerCtx *ctx, int label) {
  MirInst inst;
  init_inst(&inst, MIR_OP_LABEL);
  inst.label = label;
  mir_emit(ctx->mf, &inst);
}

static void emit_jmp(LowerCtx *ctx, int label) {
  MirInst inst;
  init_inst(&inst, MIR_OP_JMP);
  inst.label = label;
  mir_emit(ctx->mf, &inst);
}

static void emit_jz(LowerCtx *ctx, VReg cond, Type *type, int label) {
  MirInst inst;
  init_inst(&inst, MIR_OP_JZ);
  inst.src1 = cond;
  inst.type = type;
  inst.label = label;
  mir_emit(ctx->mf, &inst);
}

static VReg lower_expr(LowerCtx *ctx, Node *node);

static void emit_jcc(LowerCtx *ctx, VReg lhs, VReg rhs, Type *type, MirCondCode cc, int label) {
  MirInst inst;
  init_inst(&inst, MIR_OP_JCC);
  inst.src1 = lhs;
  inst.src2 = rhs;
  inst.type = type;
  inst.imm = cc;
  inst.label = label;
  mir_emit(ctx->mf, &inst);
}

static Type *pick_compare_type(const Node *node) {
  if (!node)
    return NULL;
  Type *cmp_type = node->type;
  if (node->lhs && node->rhs && node->lhs->type && node->rhs->type) {
    if (is_number(node->lhs->type) && is_number(node->rhs->type))
      cmp_type = max_type(node->lhs->type, node->rhs->type);
    else if (node->lhs->type->is_unsigned)
      cmp_type = node->lhs->type;
    else if (node->rhs->type->is_unsigned)
      cmp_type = node->rhs->type;
  }
  return cmp_type;
}

static int select_compare_false_cc(NodeKind kind, MirCondCode *cc) {
  if (!cc)
    return 0;
  switch (kind) {
  case ND_EQ:
    *cc = MIR_CC_NE;
    return 1;
  case ND_NE:
    *cc = MIR_CC_EQ;
    return 1;
  case ND_LT:
    *cc = MIR_CC_GE;
    return 1;
  case ND_LE:
    *cc = MIR_CC_GT;
    return 1;
  default:
    return 0;
  }
}

static int select_compare_true_cc(NodeKind kind, MirCondCode *cc) {
  if (!cc)
    return 0;
  switch (kind) {
  case ND_EQ:
    *cc = MIR_CC_EQ;
    return 1;
  case ND_NE:
    *cc = MIR_CC_NE;
    return 1;
  case ND_LT:
    *cc = MIR_CC_LT;
    return 1;
  case ND_LE:
    *cc = MIR_CC_LE;
    return 1;
  default:
    return 0;
  }
}

static void emit_jmp_if_false_fallback(LowerCtx *ctx, Node *cond, int label) {
  VReg v = lower_expr(ctx, cond);
  emit_jz(ctx, v, cond ? cond->type : NULL, label);
}

static void emit_jmp_if_true_fallback(LowerCtx *ctx, Node *cond, int label) {
  int l_end = new_label(ctx);
  VReg v = lower_expr(ctx, cond);
  emit_jz(ctx, v, cond ? cond->type : NULL, l_end);
  emit_jmp(ctx, label);
  emit_label(ctx, l_end);
}

static int try_emit_jmp_if_true_cond(LowerCtx *ctx, Node *cond, int label);

static int try_emit_jmp_if_false_cond(LowerCtx *ctx, Node *cond, int label) {
  if (!ctx || !cond)
    return 0;

  MirCondCode cc;
  if (select_compare_false_cc(cond->kind, &cc)) {
    VReg lhs = lower_expr(ctx, cond->lhs);
    VReg rhs = lower_expr(ctx, cond->rhs);
    emit_jcc(ctx, lhs, rhs, pick_compare_type(cond), cc, label);
    return 1;
  }

  switch (cond->kind) {
  case ND_NOT:
    if (!cond->lhs)
      return 0;
    if (!try_emit_jmp_if_true_cond(ctx, cond->lhs, label))
      emit_jmp_if_true_fallback(ctx, cond->lhs, label);
    return 1;
  case ND_AND:
    if (!cond->lhs || !cond->rhs)
      return 0;
    if (!try_emit_jmp_if_false_cond(ctx, cond->lhs, label))
      emit_jmp_if_false_fallback(ctx, cond->lhs, label);
    if (!try_emit_jmp_if_false_cond(ctx, cond->rhs, label))
      emit_jmp_if_false_fallback(ctx, cond->rhs, label);
    return 1;
  case ND_OR: {
    if (!cond->lhs || !cond->rhs)
      return 0;
    int l_eval_rhs = new_label(ctx);
    int l_end = new_label(ctx);
    if (!try_emit_jmp_if_false_cond(ctx, cond->lhs, l_eval_rhs))
      emit_jmp_if_false_fallback(ctx, cond->lhs, l_eval_rhs);
    emit_jmp(ctx, l_end);
    emit_label(ctx, l_eval_rhs);
    if (!try_emit_jmp_if_false_cond(ctx, cond->rhs, label))
      emit_jmp_if_false_fallback(ctx, cond->rhs, label);
    emit_label(ctx, l_end);
    return 1;
  }
  default:
    return 0;
  }
}

static int try_emit_jmp_if_true_cond(LowerCtx *ctx, Node *cond, int label) {
  if (!ctx || !cond)
    return 0;

  MirCondCode cc;
  if (select_compare_true_cc(cond->kind, &cc)) {
    VReg lhs = lower_expr(ctx, cond->lhs);
    VReg rhs = lower_expr(ctx, cond->rhs);
    emit_jcc(ctx, lhs, rhs, pick_compare_type(cond), cc, label);
    return 1;
  }

  switch (cond->kind) {
  case ND_NOT:
    if (!cond->lhs)
      return 0;
    if (!try_emit_jmp_if_false_cond(ctx, cond->lhs, label))
      emit_jmp_if_false_fallback(ctx, cond->lhs, label);
    return 1;
  case ND_AND: {
    if (!cond->lhs || !cond->rhs)
      return 0;
    int l_end = new_label(ctx);
    if (!try_emit_jmp_if_false_cond(ctx, cond->lhs, l_end))
      emit_jmp_if_false_fallback(ctx, cond->lhs, l_end);
    if (!try_emit_jmp_if_true_cond(ctx, cond->rhs, label))
      emit_jmp_if_true_fallback(ctx, cond->rhs, label);
    emit_label(ctx, l_end);
    return 1;
  }
  case ND_OR:
    if (!cond->lhs || !cond->rhs)
      return 0;
    if (!try_emit_jmp_if_true_cond(ctx, cond->lhs, label))
      emit_jmp_if_true_fallback(ctx, cond->lhs, label);
    if (!try_emit_jmp_if_true_cond(ctx, cond->rhs, label))
      emit_jmp_if_true_fallback(ctx, cond->rhs, label);
    return 1;
  default:
    return 0;
  }
}

static inline int is_main_function(const Function *fn) { return fn && fn->len == 4 && !strncmp(fn->name, "main", 4); }

static int mir_has_ret(const MirFunction *mf) {
  if (!mf)
    return 0;
  for (int i = 0; i < mf->inst_len; i++) {
    if (mf->insts[i].op == MIR_OP_RET)
      return 1;
  }
  return 0;
}

static int mir_end_reachable(const MirFunction *mf) {
  if (!mf || mf->inst_len <= 0)
    return 1;

  int n = mf->inst_len;
  int *label_to_inst = NULL;
  if (mf->next_label > 0) {
    label_to_inst = malloc(sizeof(int) * mf->next_label);
    if (!label_to_inst)
      error("memory allocation failed [in mir_end_reachable label map]");
    for (int l = 0; l < mf->next_label; l++)
      label_to_inst[l] = -1;
    for (int i = 0; i < n; i++) {
      if (mf->insts[i].op != MIR_OP_LABEL)
        continue;
      int l = mf->insts[i].label;
      if (l >= 0 && l < mf->next_label)
        label_to_inst[l] = i;
    }
  }

  unsigned char *visited = calloc((size_t)n + 1, sizeof(unsigned char));
  int *queue = malloc(sizeof(int) * ((size_t)n + 1));
  if (!visited || !queue)
    error("memory allocation failed [in mir_end_reachable worklist]");

  int head = 0;
  int tail = 0;
  queue[tail++] = 0;
  visited[0] = 1;

  while (head < tail) {
    int i = queue[head++];
    if (i == n)
      continue;

    MirInst *in = &mf->insts[i];
    int succ0 = -1;
    int succ1 = -1;
    int fall = (i + 1 <= n) ? (i + 1) : -1;
    if (in->op == MIR_OP_RET) {
      succ0 = -1;
    } else if (in->op == MIR_OP_JMP) {
      if (label_to_inst && in->label >= 0 && in->label < mf->next_label)
        succ0 = label_to_inst[in->label];
    } else if (in->op == MIR_OP_JZ || in->op == MIR_OP_JCC) {
      succ0 = fall;
      if (label_to_inst && in->label >= 0 && in->label < mf->next_label)
        succ1 = label_to_inst[in->label];
    } else {
      succ0 = fall;
    }

    if (succ0 >= 0 && succ0 <= n && !visited[succ0]) {
      visited[succ0] = 1;
      queue[tail++] = succ0;
    }
    if (succ1 >= 0 && succ1 <= n && !visited[succ1]) {
      visited[succ1] = 1;
      queue[tail++] = succ1;
    }
  }

  int reachable = visited[n] != 0;
  free(queue);
  free(visited);
  free(label_to_inst);
  return reachable;
}

static void emit_implicit_return_if_needed(LowerCtx *ctx, Node *node) {
  if (!ctx || !ctx->mf || !ctx->fn || !ctx->fn->type || !ctx->fn->type->return_type)
    lower_error_node("invalid function in implicit return lowering", node);

  Type *ret_type = ctx->fn->type->return_type;
  if (ret_type->ty != TY_VOID && !is_main_function(ctx->fn))
    return;
  int has_ret = mir_has_ret(ctx->mf);
  int need_implicit_ret = !has_ret || mir_end_reachable(ctx->mf);
  if (!need_implicit_ret)
    return;

  MirInst inst;
  init_inst(&inst, MIR_OP_RET);
  if (is_main_function(ctx->fn)) {
    VReg zero = mir_new_vreg(ctx->mf);
    MirInst imm;
    init_inst(&imm, MIR_OP_IMM);
    imm.dst = zero;
    imm.imm = 0;
    imm.type = ret_type;
    mir_emit(ctx->mf, &imm);
    inst.src1 = zero;
  }
  inst.type = ret_type;
  mir_emit(ctx->mf, &inst);
}

static VReg lower_expr_impl(LowerCtx *ctx, Node *node);
static VReg lower_expr(LowerCtx *ctx, Node *node);
static void lower_expr_discard(LowerCtx *ctx, Node *node);
static void lower_stmt(LowerCtx *ctx, Node *node);

static inline VReg lower_expr(LowerCtx *ctx, Node *node) { return lower_expr_impl(ctx, node); }

static inline void lower_expr_discard(LowerCtx *ctx, Node *node) { (void)lower_expr(ctx, node); }

static int cast_is_noop(Type *from, Type *to) {
  if (!from || !to)
    return false;

  if (to->ty == TY_BOOL)
    return false;

  if (is_ptr_or_arr(from) && is_ptr_or_arr(to))
    return true;

  if (is_number(from) && is_number(to))
    return type_size(from) == type_size(to);

  if (is_ptr_or_arr(from) && is_number(to))
    return type_size(from) == type_size(to);

  if (is_number(from) && is_ptr_or_arr(to))
    return type_size(from) == type_size(to);

  return false;
}

static void push_control(LowerCtx *ctx, int break_label, int continue_label, Node *node) {
  if (ctx->ctrl_depth >= MAX_CTRL_DEPTH)
    lower_error_node("control nesting too deep in lowering", node);
  ctx->ctrl_stack[ctx->ctrl_depth].break_label = break_label;
  ctx->ctrl_stack[ctx->ctrl_depth].continue_label = continue_label;
  ctx->ctrl_depth++;
}

static void pop_control(LowerCtx *ctx, Node *node) {
  if (ctx->ctrl_depth <= 0)
    lower_error_node("invalid control stack pop in lowering", node);
  ctx->ctrl_depth--;
}

static int find_break_target(LowerCtx *ctx, Node *node) {
  for (int i = ctx->ctrl_depth - 1; i >= 0; i--) {
    if (ctx->ctrl_stack[i].break_label != MIR_INVALID_LABEL)
      return ctx->ctrl_stack[i].break_label;
  }
  lower_error_node("break outside of loop/switch in lowering", node);
  return MIR_INVALID_LABEL;
}

static int find_continue_target(LowerCtx *ctx, Node *node) {
  for (int i = ctx->ctrl_depth - 1; i >= 0; i--) {
    if (ctx->ctrl_stack[i].continue_label != MIR_INVALID_LABEL)
      return ctx->ctrl_stack[i].continue_label;
  }
  lower_error_node("continue outside of loop in lowering", node);
  return MIR_INVALID_LABEL;
}

static Label *find_function_label(Function *fn, const char *name, int len) {
  for (Label *label = fn ? fn->labels : NULL; label; label = label->next) {
    if (label->len == len && !strncmp(label->name, name, len))
      return label;
  }
  return NULL;
}

static int find_mir_label(LowerCtx *ctx, int ast_label_id) {
  for (AstLabelMap *it = ctx->label_map; it; it = it->next) {
    if (it->ast_label_id == ast_label_id)
      return it->mir_label;
  }
  return MIR_INVALID_LABEL;
}

static int create_mir_label(LowerCtx *ctx, int ast_label_id, Node *node) {
  if (find_mir_label(ctx, ast_label_id) != MIR_INVALID_LABEL)
    lower_error_node("duplicate MIR label mapping in lowering", node);

  AstLabelMap *entry = malloc(sizeof(*entry));
  if (!entry)
    error("memory allocation failed [in create_mir_label]");
  entry->ast_label_id = ast_label_id;
  entry->mir_label = new_label(ctx);
  entry->next = ctx->label_map;
  ctx->label_map = entry;
  return entry->mir_label;
}

static void seed_function_label_map(LowerCtx *ctx, Function *fn, Node *node) {
  for (Label *label = fn ? fn->labels : NULL; label; label = label->next)
    (void)create_mir_label(ctx, label->id, node);
}

static void free_label_map(AstLabelMap *map) {
  while (map) {
    AstLabelMap *next = map->next;
    free(map);
    map = next;
  }
}

static SwitchCaseMap *find_switch_case_map(const SwitchCtx *sw, int case_value) {
  for (SwitchCaseMap *it = sw ? sw->cases : NULL; it; it = it->next) {
    if (it->case_value == case_value)
      return it;
  }
  return NULL;
}

static void add_switch_case_map(SwitchCtx *sw, int case_value, int case_label) {
  SwitchCaseMap *entry = malloc(sizeof(*entry));
  if (!entry)
    error("memory allocation failed [in add_switch_case_map]");
  entry->case_value = case_value;
  entry->case_label = case_label;
  entry->next = sw->cases;
  sw->cases = entry;
}

static void free_switch_case_map(SwitchCaseMap *map) {
  while (map) {
    SwitchCaseMap *next = map->next;
    free(map);
    map = next;
  }
}

static SwitchCtx *find_switch_ctx(LowerCtx *ctx, int switch_id) {
  for (SwitchCtx *it = ctx->switch_stack; it; it = it->next) {
    if (it->switch_id == switch_id)
      return it;
  }
  return NULL;
}

static SwitchCtx *push_switch_ctx(LowerCtx *ctx, int switch_id, int end_label, int default_label) {
  SwitchCtx *sw = malloc(sizeof(*sw));
  if (!sw)
    error("memory allocation failed [in push_switch_ctx]");
  sw->switch_id = switch_id;
  sw->end_label = end_label;
  sw->default_label = default_label;
  sw->cases = NULL;
  sw->next = ctx->switch_stack;
  ctx->switch_stack = sw;
  return sw;
}

static void pop_switch_ctx(LowerCtx *ctx, Node *node) {
  if (!ctx->switch_stack)
    lower_error_node("invalid switch stack pop in lowering", node);
  SwitchCtx *sw = ctx->switch_stack;
  ctx->switch_stack = sw->next;
  free_switch_case_map(sw->cases);
  free(sw);
}

static void collect_switch_cases(Node *node, int switch_id, int **cases, int *case_cnt, int *case_cap) {
  if (!node)
    return;

  if (node->kind == ND_CASE && node->id == switch_id) {
    for (int i = 0; i < *case_cnt; i++) {
      if ((*cases)[i] == node->val)
        return;
    }
    *cases = safe_realloc_array(*cases, sizeof(int), *case_cnt + 1, case_cap);
    (*cases)[(*case_cnt)++] = node->val;
    return;
  }

  switch (node->kind) {
  case ND_BLOCK:
    for (int i = 0; node->body && node->body[i] && node->body[i]->kind != ND_NONE; i++)
      collect_switch_cases(node->body[i], switch_id, cases, case_cnt, case_cap);
    break;
  case ND_IF:
  case ND_TERNARY:
    collect_switch_cases(node->cond, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->then, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->els, switch_id, cases, case_cnt, case_cap);
    break;
  case ND_WHILE:
  case ND_DOWHILE:
  case ND_FOR:
    collect_switch_cases(node->init, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->cond, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->then, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->step, switch_id, cases, case_cnt, case_cap);
    break;
  case ND_SWITCH:
    collect_switch_cases(node->cond, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->then, switch_id, cases, case_cnt, case_cap);
    break;
  case ND_TYPECAST:
    collect_switch_cases(node->lhs, switch_id, cases, case_cnt, case_cap);
    break;
  case ND_FUNCALL:
    collect_switch_cases(node->lhs, switch_id, cases, case_cnt, case_cap);
    for (int i = 0; i < MAX_FUNC_PARAMS && node->args[i]; i++)
      collect_switch_cases(node->args[i], switch_id, cases, case_cnt, case_cap);
    break;
  default:
    collect_switch_cases(node->lhs, switch_id, cases, case_cnt, case_cap);
    collect_switch_cases(node->rhs, switch_id, cases, case_cnt, case_cap);
    break;
  }
}

static VReg lower_imm(LowerCtx *ctx, long imm, Type *type) {
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, MIR_OP_IMM);
  inst.dst = dst;
  inst.imm = imm;
  inst.type = type;
  mir_emit(ctx->mf, &inst);
  return dst;
}

static void emit_mov(LowerCtx *ctx, VReg dst, VReg src, Type *type) {
  MirInst inst;
  init_inst(&inst, MIR_OP_MOV);
  inst.dst = dst;
  inst.src1 = src;
  inst.type = type;
  mir_emit(ctx->mf, &inst);
}

static VReg lower_not(LowerCtx *ctx, Node *node) {
  VReg src = lower_expr(ctx, node->lhs);
  VReg zero = lower_imm(ctx, 0, node->type);
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst eq;
  init_inst(&eq, MIR_OP_EQ);
  eq.dst = dst;
  eq.src1 = src;
  eq.src2 = zero;
  eq.type = node->type;
  mir_emit(ctx->mf, &eq);
  return dst;
}

static VReg lower_logical_and(LowerCtx *ctx, Node *node) {
  VReg dst = mir_new_vreg(ctx->mf);
  VReg zero = lower_imm(ctx, 0, node->type);
  VReg one = lower_imm(ctx, 1, node->type);
  int l_end = new_label(ctx);

  emit_mov(ctx, dst, zero, node->type);

  VReg lhs = lower_expr(ctx, node->lhs);
  emit_jz(ctx, lhs, node->lhs ? node->lhs->type : NULL, l_end);

  VReg rhs = lower_expr(ctx, node->rhs);
  emit_jz(ctx, rhs, node->rhs ? node->rhs->type : NULL, l_end);

  emit_mov(ctx, dst, one, node->type);
  emit_label(ctx, l_end);
  return dst;
}

static VReg lower_logical_or(LowerCtx *ctx, Node *node) {
  VReg dst = mir_new_vreg(ctx->mf);
  VReg zero = lower_imm(ctx, 0, node->type);
  VReg one = lower_imm(ctx, 1, node->type);
  int l_eval_rhs = new_label(ctx);
  int l_end = new_label(ctx);

  emit_mov(ctx, dst, zero, node->type);

  VReg lhs = lower_expr(ctx, node->lhs);
  emit_jz(ctx, lhs, node->lhs ? node->lhs->type : NULL, l_eval_rhs);
  emit_mov(ctx, dst, one, node->type);
  emit_jmp(ctx, l_end);

  emit_label(ctx, l_eval_rhs);
  VReg rhs = lower_expr(ctx, node->rhs);
  emit_jz(ctx, rhs, node->rhs ? node->rhs->type : NULL, l_end);
  emit_mov(ctx, dst, one, node->type);

  emit_label(ctx, l_end);
  return dst;
}

static VReg lower_ternary(LowerCtx *ctx, Node *node) {
  if (!node->cond || !node->then || !node->els)
    lower_error_node("invalid ternary node in lowering", node);

  int l_else = new_label(ctx);
  int l_end = new_label(ctx);

  if (node->type && node->type->ty == TY_VOID) {
    if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_else)) {
      VReg cond = lower_expr(ctx, node->cond);
      emit_jz(ctx, cond, node->cond ? node->cond->type : NULL, l_else);
    }
    lower_expr_discard(ctx, node->then);
    emit_jmp(ctx, l_end);
    emit_label(ctx, l_else);
    lower_expr_discard(ctx, node->els);
    emit_label(ctx, l_end);
    return MIR_INVALID_VREG;
  }

  VReg dst = mir_new_vreg(ctx->mf);
  if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_else)) {
    VReg cond = lower_expr(ctx, node->cond);
    emit_jz(ctx, cond, node->cond ? node->cond->type : NULL, l_else);
  }
  VReg then_v = lower_expr(ctx, node->then);
  emit_mov(ctx, dst, then_v, node->type);
  emit_jmp(ctx, l_end);
  emit_label(ctx, l_else);
  VReg else_v = lower_expr(ctx, node->els);
  emit_mov(ctx, dst, else_v, node->type);
  emit_label(ctx, l_end);
  return dst;
}

static VReg lower_addr(LowerCtx *ctx, Node *node) {
  if (!ctx || !ctx->mf || !node)
    lower_error_node("invalid address lowering context", node);

  switch (node->kind) {
  case ND_LVAR:
  case ND_VARDEC:
    if (!node->var)
      lower_error_node("local variable node has no symbol in lowering", node);
    break;
  case ND_GVAR:
  case ND_GLBDEC:
    if (!node->var)
      lower_error_node("global variable node has no symbol in lowering", node);
    break;
  case ND_DEREF:
    return lower_expr(ctx, node->lhs);
  default:
    lower_error_node("expression is not addressable in lowering", node);
  }

  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, MIR_OP_ADDR_SYMBOL);
  inst.dst = dst;
  inst.type = node->type;
  inst.var = node->var;
  if (node->kind == ND_LVAR || node->kind == ND_VARDEC) {
    if (!node->var->is_static) {
      inst.op = MIR_OP_ADDR_LOCAL;
      inst.offset = node->var->offset;
      inst.var = NULL;
    }
  }
  mir_emit(ctx->mf, &inst);
  return dst;
}

static VReg lower_load_from_addr(LowerCtx *ctx, VReg addr, Type *type) {
  if (type && (type->ty == TY_ARR || type->ty == TY_STRUCT || type->ty == TY_UNION))
    return addr;

  VReg dst = mir_new_vreg(ctx->mf);
  MirInst load;
  init_inst(&load, MIR_OP_LOAD);
  load.dst = dst;
  load.src1 = addr;
  load.type = type;
  mir_emit(ctx->mf, &load);
  return dst;
}

static void lower_store_to_addr(LowerCtx *ctx, VReg addr, VReg value, Type *type) {
  MirInst store;
  init_inst(&store, MIR_OP_STORE);
  store.src1 = addr;
  store.src2 = value;
  store.type = type;
  mir_emit(ctx->mf, &store);
}

static VReg lower_cast_value_if_needed(LowerCtx *ctx, VReg src, Type *from, Type *to) {
  if (src == MIR_INVALID_VREG)
    return MIR_INVALID_VREG;
  if (!to)
    return src;
  if (cast_is_noop(from, to))
    return src;

  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, MIR_OP_CAST);
  inst.dst = dst;
  inst.src1 = src;
  inst.type = to;
  mir_emit(ctx->mf, &inst);
  return dst;
}

static VReg emit_binary_inst(LowerCtx *ctx, MirOp op, VReg lhs, VReg rhs, Type *type) {
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, op);
  inst.dst = dst;
  inst.src1 = lhs;
  inst.src2 = rhs;
  inst.type = type;
  mir_emit(ctx->mf, &inst);
  return dst;
}

static VReg lower_postinc_new_from_old(LowerCtx *ctx, const Node *node, VReg old_value) {
  Node *rhs = node->rhs;
  MirOp op = rhs->kind == ND_ADD ? MIR_OP_ADD : MIR_OP_SUB;
  VReg step = lower_expr(ctx, rhs->rhs);
  Type *calc_type = rhs->type ? rhs->type : (node->lhs ? node->lhs->type : node->type);
  return emit_binary_inst(ctx, op, old_value, step, calc_type);
}

static void emit_memcpy(LowerCtx *ctx, VReg dst_addr, VReg src_addr, int size) {
  if (size < 0)
    error("invalid memcpy size in lowering");
  if (size == 0)
    return;

  MirInst inst;
  init_inst(&inst, MIR_OP_MEMCPY);
  inst.src1 = dst_addr;
  inst.src2 = src_addr;
  inst.imm = size;
  mir_emit(ctx->mf, &inst);
}

static VReg lower_postinc(LowerCtx *ctx, Node *node) {
  if (!node->lhs || !node->rhs)
    lower_error_node("invalid postinc node in lowering", node);

  VReg addr = lower_addr(ctx, node->lhs);
  VReg old_value = MIR_INVALID_VREG;
  old_value = lower_load_from_addr(ctx, addr, node->lhs->type);
  VReg new_value = lower_postinc_new_from_old(ctx, node, old_value);
  lower_store_to_addr(ctx, addr, new_value, node->lhs->type);
  return old_value;
}

static VReg lower_binary(LowerCtx *ctx, Node *node, MirOp op) {
  VReg lhs = lower_expr(ctx, node->lhs);
  VReg rhs = lower_expr(ctx, node->rhs);
  return emit_binary_inst(ctx, op, lhs, rhs, node->type);
}

static VReg lower_compare(LowerCtx *ctx, Node *node, MirOp op) {
  VReg lhs = lower_expr(ctx, node->lhs);
  VReg rhs = lower_expr(ctx, node->rhs);
  return emit_binary_inst(ctx, op, lhs, rhs, pick_compare_type(node));
}

static VReg lower_unary(LowerCtx *ctx, Node *node, MirOp op) {
  VReg src = lower_expr(ctx, node->lhs);
  VReg dst = mir_new_vreg(ctx->mf);

  MirInst inst;
  init_inst(&inst, op);
  inst.dst = dst;
  inst.src1 = src;
  inst.type = node->type;
  mir_emit(ctx->mf, &inst);
  return dst;
}

static VReg lower_const_addr(LowerCtx *ctx, Node *node, MirOp op) {
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, op);
  inst.dst = dst;
  inst.offset = node->id;
  inst.type = node->type;
  mir_emit(ctx->mf, &inst);
  return dst;
}

static VReg lower_call(LowerCtx *ctx, Node *node) {
  MirInst call;
  init_inst(&call, MIR_OP_CALL);

  if (node->val > MAX_FUNC_PARAMS)
    lower_error_node("too many function arguments in lowering", node);

  for (int i = 0; i < node->val; i++)
    call.args[i] = lower_expr(ctx, node->args[i]);

  call.argc = node->val;
  call.type = node->type;

  if (node->fn) {
    // 関数の直接呼び出し
    call.call_fn = node->fn;
  } else {
    // 関数ポインタ呼び出し
    call.src1 = lower_expr(ctx, node->lhs);
  }

  if (node->type && node->type->ty != TY_VOID)
    call.dst = mir_new_vreg(ctx->mf);

  mir_emit(ctx->mf, &call);
  return call.dst;
}

static void emit_jmp_if_eq(LowerCtx *ctx, VReg lhs, VReg rhs, Type *type, int label) {
  emit_jcc(ctx, lhs, rhs, type, MIR_CC_EQ, label);
}

static void lower_switch_stmt(LowerCtx *ctx, Node *node) {
  if (!node->cond || !node->then)
    lower_error_node("invalid switch node in lowering", node);

  VReg cond = lower_expr(ctx, node->cond);
  int l_end = new_label(ctx);
  int l_default = node->has_default ? new_label(ctx) : l_end;

  SwitchCtx *sw = push_switch_ctx(ctx, node->id, l_end, l_default);

  int *cases = NULL;
  int case_cnt = 0;
  int case_cap = 0;
  collect_switch_cases(node->then, node->id, &cases, &case_cnt, &case_cap);
  for (int i = 0; i < case_cnt; i++) {
    if (find_switch_case_map(sw, cases[i]))
      lower_error_node("duplicate switch case value in lowering", node);
    add_switch_case_map(sw, cases[i], new_label(ctx));
  }

  for (int i = 0; i < case_cnt; i++) {
    SwitchCaseMap *cm = find_switch_case_map(sw, cases[i]);
    if (!cm)
      lower_error_node("switch case label not found in lowering", node);
    VReg case_value = lower_imm(ctx, cases[i], node->cond->type);
    emit_jmp_if_eq(ctx, cond, case_value, node->cond->type, cm->case_label);
  }

  free(cases);
  cases = NULL;
  emit_jmp(ctx, l_default);

  push_control(ctx, l_end, MIR_INVALID_LABEL, node);
  lower_stmt(ctx, node->then);
  pop_control(ctx, node);

  emit_label(ctx, l_end);
  pop_switch_ctx(ctx, node);
}

static VReg lower_expr_impl(LowerCtx *ctx, Node *node) {
  if (!ctx || !ctx->mf || !node)
    lower_error_node("invalid lowering context", node);

  switch (node->kind) {
  case ND_NUM: {
    return lower_imm(ctx, node->val, node->type);
  }
  case ND_LVAR:
  case ND_VARDEC:
  case ND_GVAR:
  case ND_GLBDEC: {
    VReg addr = lower_addr(ctx, node);
    return lower_load_from_addr(ctx, addr, node->type);
  }
  case ND_FUNCNAME: {
    if (!node->fn)
      lower_error_node("function name node has no function in lowering", node);
    VReg dst = mir_new_vreg(ctx->mf);
    MirInst inst;
    init_inst(&inst, MIR_OP_ADDR_FUNC);
    inst.dst = dst;
    inst.call_fn = node->fn;
    inst.type = node->type;
    mir_emit(ctx->mf, &inst);
    return dst;
  }
  case ND_ADDR:
    if (!node->lhs)
      lower_error_node("address operator has no operand in lowering", node);
    if (node->lhs->kind == ND_FUNCNAME)
      return lower_expr(ctx, node->lhs);
    return lower_addr(ctx, node->lhs);
  case ND_DEREF: {
    VReg addr = lower_expr(ctx, node->lhs);
    return lower_load_from_addr(ctx, addr, node->type);
  }
  case ND_STRING:
    return lower_const_addr(ctx, node, MIR_OP_ADDR_STRING);
  case ND_ARRAY:
    return lower_const_addr(ctx, node, MIR_OP_ADDR_ARRAY);
  case ND_STRUCT_LITERAL:
    return lower_const_addr(ctx, node, MIR_OP_ADDR_STRUCT_LITERAL);
  case ND_ADD:
    return lower_binary(ctx, node, MIR_OP_ADD);
  case ND_SUB:
    return lower_binary(ctx, node, MIR_OP_SUB);
  case ND_MUL:
    return lower_binary(ctx, node, MIR_OP_MUL);
  case ND_DIV:
    return lower_binary(ctx, node, (node->type && node->type->is_unsigned) ? MIR_OP_UDIV : MIR_OP_SDIV);
  case ND_MOD:
    return lower_binary(ctx, node, (node->type && node->type->is_unsigned) ? MIR_OP_UMOD : MIR_OP_SMOD);
  case ND_EQ:
    return lower_binary(ctx, node, MIR_OP_EQ);
  case ND_NE:
    return lower_binary(ctx, node, MIR_OP_NE);
  case ND_LT:
    return lower_compare(ctx, node, MIR_OP_LT);
  case ND_LE:
    return lower_compare(ctx, node, MIR_OP_LE);
  case ND_BITAND:
    return lower_binary(ctx, node, MIR_OP_BITAND);
  case ND_BITOR:
    return lower_binary(ctx, node, MIR_OP_BITOR);
  case ND_BITXOR:
    return lower_binary(ctx, node, MIR_OP_BITXOR);
  case ND_SHL:
    return lower_binary(ctx, node, MIR_OP_SHL);
  case ND_SHR:
    return lower_binary(ctx, node, MIR_OP_SHR);
  case ND_AND:
    return lower_logical_and(ctx, node);
  case ND_OR:
    return lower_logical_or(ctx, node);
  case ND_NOT:
    return lower_not(ctx, node);
  case ND_BITNOT:
    return lower_unary(ctx, node, MIR_OP_BITNOT);
  case ND_TERNARY:
    return lower_ternary(ctx, node);
  case ND_ASSIGN: {
    if (node->val) {
      VReg dst_addr = lower_addr(ctx, node->lhs);
      VReg src_addr = lower_expr(ctx, node->rhs);
      emit_memcpy(ctx, dst_addr, src_addr, get_sizeof(node->lhs->type));
      return src_addr;
    }
    if (node->lhs && node->lhs->type && (node->lhs->type->ty == TY_STRUCT || node->lhs->type->ty == TY_UNION)) {
      VReg dst_addr = lower_addr(ctx, node->lhs);
      VReg src_addr = lower_addr(ctx, node->rhs);
      emit_memcpy(ctx, dst_addr, src_addr, get_sizeof(node->lhs->type));
      return src_addr;
    }
    VReg addr = lower_addr(ctx, node->lhs);
    VReg rhs = lower_expr(ctx, node->rhs);
    Type *lhs_type = node->lhs ? node->lhs->type : node->type;
    VReg stored = rhs;
    if (lhs_type && lhs_type->ty != TY_STRUCT && lhs_type->ty != TY_UNION && lhs_type->ty != TY_ARR)
      stored = lower_cast_value_if_needed(ctx, rhs, node->rhs ? node->rhs->type : NULL, lhs_type);
    lower_store_to_addr(ctx, addr, stored, lhs_type);

    // Scalar-like assignments already have the stored value in "stored";
    // return it directly instead of reloading from memory.
    if (lhs_type && lhs_type->ty != TY_STRUCT && lhs_type->ty != TY_UNION && lhs_type->ty != TY_ARR)
      return stored;

    // Aggregates keep address-style value flow in the current MIR design.
    return lower_load_from_addr(ctx, addr, lhs_type);
  }
  case ND_POSTINC:
    return lower_postinc(ctx, node);
  case ND_COMMA: {
    lower_expr(ctx, node->lhs);
    return lower_expr(ctx, node->rhs);
  }
  case ND_TYPECAST: {
    VReg src = lower_expr(ctx, node->lhs);
    Type *from = node->lhs ? node->lhs->type : NULL;
    return lower_cast_value_if_needed(ctx, src, from, node->type);
  }
  case ND_FUNCALL:
    return lower_call(ctx, node);
  default:
    lower_error_node("unsupported expression node in lowering", node);
  }

  return MIR_INVALID_VREG;
}

static void lower_stmt(LowerCtx *ctx, Node *node) {
  if (!ctx || !node)
    lower_error_node("invalid statement lowering context", node);

  switch (node->kind) {
  case ND_NONE:
    return;
  case ND_BLOCK:
    for (int i = 0; node->body && node->body[i] && node->body[i]->kind != ND_NONE; i++)
      lower_stmt(ctx, node->body[i]);
    return;
  case ND_RETURN: {
    MirInst inst;
    init_inst(&inst, MIR_OP_RET);
    Type *ret_type = (ctx->fn && ctx->fn->type) ? ctx->fn->type->return_type : node->type;
    if (!ret_type || ret_type->ty != TY_VOID) {
      VReg ret_v = lower_expr(ctx, node->rhs);
      ret_v = lower_cast_value_if_needed(ctx, ret_v, node->rhs ? node->rhs->type : NULL, ret_type);
      inst.src1 = ret_v;
    }
    inst.type = ret_type;
    mir_emit(ctx->mf, &inst);
    return;
  }
  case ND_IF: {
    int l_end = new_label(ctx);
    if (node->els) {
      int l_else = new_label(ctx);
      if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_else))
        emit_jmp_if_false_fallback(ctx, node->cond, l_else);
      lower_stmt(ctx, node->then);
      emit_jmp(ctx, l_end);
      emit_label(ctx, l_else);
      lower_stmt(ctx, node->els);
      emit_label(ctx, l_end);
    } else {
      if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_end))
        emit_jmp_if_false_fallback(ctx, node->cond, l_end);
      lower_stmt(ctx, node->then);
      emit_label(ctx, l_end);
    }
    return;
  }
  case ND_WHILE: {
    int l_begin = new_label(ctx);
    int l_end = new_label(ctx);
    emit_label(ctx, l_begin);
    if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_end))
      emit_jmp_if_false_fallback(ctx, node->cond, l_end);

    push_control(ctx, l_end, l_begin, node);
    lower_stmt(ctx, node->then);
    pop_control(ctx, node);

    emit_jmp(ctx, l_begin);
    emit_label(ctx, l_end);
    return;
  }
  case ND_DOWHILE: {
    int l_begin = new_label(ctx);
    int l_step = new_label(ctx);
    int l_end = new_label(ctx);
    emit_label(ctx, l_begin);

    push_control(ctx, l_end, l_step, node);
    lower_stmt(ctx, node->then);
    pop_control(ctx, node);

    emit_label(ctx, l_step);
    if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_end))
      emit_jmp_if_false_fallback(ctx, node->cond, l_end);
    emit_jmp(ctx, l_begin);
    emit_label(ctx, l_end);
    return;
  }
  case ND_FOR: {
    if (node->init && node->init->kind != ND_NONE)
      lower_stmt(ctx, node->init);

    int l_begin = new_label(ctx);
    int l_step = new_label(ctx);
    int l_end = new_label(ctx);
    emit_label(ctx, l_begin);

    if (node->cond && node->cond->kind != ND_NONE) {
      if (!try_emit_jmp_if_false_cond(ctx, node->cond, l_end))
        emit_jmp_if_false_fallback(ctx, node->cond, l_end);
    }

    push_control(ctx, l_end, l_step, node);
    lower_stmt(ctx, node->then);
    pop_control(ctx, node);

    emit_label(ctx, l_step);
    if (node->step && node->step->kind != ND_NONE)
      lower_stmt(ctx, node->step);
    emit_jmp(ctx, l_begin);
    emit_label(ctx, l_end);
    return;
  }
  case ND_BREAK:
    emit_jmp(ctx, find_break_target(ctx, node));
    return;
  case ND_CONTINUE:
    emit_jmp(ctx, find_continue_target(ctx, node));
    return;
  case ND_GOTO: {
    if (!node->fn || !node->label)
      lower_error_node("invalid goto node in lowering", node);
    Label *target = find_function_label(node->fn, node->label->name, node->label->len);
    if (!target)
      lower_error_node("undefined label in goto lowering", node);
    int mir_label = find_mir_label(ctx, target->id);
    if (mir_label == MIR_INVALID_LABEL)
      lower_error_node("label mapping not found in goto lowering", node);
    emit_jmp(ctx, mir_label);
    return;
  }
  case ND_LABEL:
    if (!node->label)
      lower_error_node("invalid label node in lowering", node);
    int mir_label = find_mir_label(ctx, node->label->id);
    if (mir_label == MIR_INVALID_LABEL)
      lower_error_node("label mapping not found in label lowering", node);
    emit_label(ctx, mir_label);
    return;
  case ND_SWITCH:
    lower_switch_stmt(ctx, node);
    return;
  case ND_CASE: {
    SwitchCtx *sw = find_switch_ctx(ctx, node->id);
    if (!sw)
      lower_error_node("case label outside switch in lowering", node);
    SwitchCaseMap *cm = find_switch_case_map(sw, node->val);
    if (!cm)
      lower_error_node("case label not registered in lowering", node);
    emit_label(ctx, cm->case_label);
    return;
  }
  case ND_DEFAULT: {
    SwitchCtx *sw = find_switch_ctx(ctx, node->id);
    if (!sw || sw->default_label == MIR_INVALID_LABEL)
      lower_error_node("default label outside switch in lowering", node);
    emit_label(ctx, sw->default_label);
    return;
  }
  case ND_VARDEC:
  case ND_ASM:
  case ND_EXTERN:
  case ND_TYPEDEF:
    return;
  case ND_NUM:
  case ND_LVAR:
  case ND_GVAR:
  case ND_GLBDEC:
  case ND_FUNCNAME:
  case ND_ADDR:
  case ND_DEREF:
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
  case ND_AND:
  case ND_OR:
  case ND_NOT:
  case ND_POSTINC:
  case ND_TERNARY:
  case ND_ASSIGN:
  case ND_COMMA:
  case ND_TYPECAST:
  case ND_FUNCALL:
    lower_expr_discard(ctx, node);
    return;
  default:
    lower_error_node("unsupported statement node in lowering", node);
  }
}

void lower_function(Node *fn_node, MirFunction *mf) {
  if (!fn_node || fn_node->kind != ND_FUNCDEF || !fn_node->fn || !fn_node->lhs || !mf)
    lower_error_node("invalid function node in lowering", fn_node);

  mir_init(mf, fn_node->fn);
  if (fn_node->val < 0 || fn_node->val > MAX_FUNC_PARAMS)
    lower_error_node("invalid function parameter count in lowering", fn_node);
  mf->param_count = fn_node->val;
  for (int i = 0; i < mf->param_count; i++) {
    if (!fn_node->args[i] || !fn_node->args[i]->var)
      lower_error_node("invalid function parameter node in lowering", fn_node);
    mf->param_offsets[i] = fn_node->args[i]->var->offset;
    mf->param_types[i] = fn_node->args[i]->type;
  }

  LowerCtx ctx = {0};
  ctx.mf = mf;
  ctx.fn = fn_node->fn;
  seed_function_label_map(&ctx, fn_node->fn, fn_node);
  lower_stmt(&ctx, fn_node->lhs);
  emit_implicit_return_if_needed(&ctx, fn_node);
  while (ctx.switch_stack)
    pop_switch_ctx(&ctx, fn_node);
  free_label_map(ctx.label_map);
  ctx.label_map = NULL;
}
