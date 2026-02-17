#include "codegen.h"
#include "codegen_internal.h"
#include "diagnostics.h"
#include "mir.h"
#include "opt_internal.h"
#include "platform.h"
#include "runtime.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
  MAX_CTRL_DEPTH = 128,
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

static int new_label(LowerCtx *ctx) { return mir_new_label(ctx->mf); }

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

static void emit_jz(LowerCtx *ctx, VReg cond, int label) {
  MirInst inst;
  init_inst(&inst, MIR_OP_JZ);
  inst.src1 = cond;
  inst.label = label;
  mir_emit(ctx->mf, &inst);
}

static VReg lower_expr(LowerCtx *ctx, Node *node);
static void lower_stmt(LowerCtx *ctx, Node *node);

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
  emit_jz(ctx, lhs, l_end);

  VReg rhs = lower_expr(ctx, node->rhs);
  emit_jz(ctx, rhs, l_end);

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
  emit_jz(ctx, lhs, l_eval_rhs);
  emit_mov(ctx, dst, one, node->type);
  emit_jmp(ctx, l_end);

  emit_label(ctx, l_eval_rhs);
  VReg rhs = lower_expr(ctx, node->rhs);
  emit_jz(ctx, rhs, l_end);
  emit_mov(ctx, dst, one, node->type);

  emit_label(ctx, l_end);
  return dst;
}

static VReg lower_ternary(LowerCtx *ctx, Node *node) {
  if (!node->cond || !node->then || !node->els)
    lower_error_node("invalid ternary node in lowering", node);

  VReg cond = lower_expr(ctx, node->cond);
  int l_else = new_label(ctx);
  int l_end = new_label(ctx);

  if (node->type && node->type->ty == TY_VOID) {
    emit_jz(ctx, cond, l_else);
    (void)lower_expr(ctx, node->then);
    emit_jmp(ctx, l_end);
    emit_label(ctx, l_else);
    (void)lower_expr(ctx, node->els);
    emit_label(ctx, l_end);
    return MIR_INVALID_VREG;
  }

  VReg dst = mir_new_vreg(ctx->mf);
  emit_jz(ctx, cond, l_else);
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

static Type *byte_copy_type() {
  static Type *ty = NULL;
  if (!ty) {
    ty = new_type(TY_CHAR);
    ty->is_unsigned = true;
  }
  return ty;
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

static void lower_copy_bytes(LowerCtx *ctx, VReg dst_addr, VReg src_addr, int size) {
  Type *byte_ty = byte_copy_type();
  for (int i = 0; i < size; i++) {
    VReg off = lower_imm(ctx, i, NULL);
    VReg src_p = emit_binary_inst(ctx, MIR_OP_ADD, src_addr, off, NULL);
    VReg dst_p = emit_binary_inst(ctx, MIR_OP_ADD, dst_addr, off, NULL);
    VReg one = lower_load_from_addr(ctx, src_p, byte_ty);
    lower_store_to_addr(ctx, dst_p, one, byte_ty);
  }
}

static VReg lower_postinc(LowerCtx *ctx, Node *node) {
  if (!node->lhs || !node->rhs)
    lower_error_node("invalid postinc node in lowering", node);

  VReg addr = lower_addr(ctx, node->lhs);
  VReg old_value = lower_load_from_addr(ctx, addr, node->lhs->type);
  VReg new_value = lower_expr(ctx, node->rhs);
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
  Type *cmp_type = node->type;
  if (node->lhs && node->rhs && node->lhs->type && node->rhs->type) {
    if (is_number(node->lhs->type) && is_number(node->rhs->type))
      cmp_type = max_type(node->lhs->type, node->rhs->type);
    else if (node->lhs->type->is_unsigned)
      cmp_type = node->lhs->type;
    else if (node->rhs->type->is_unsigned)
      cmp_type = node->rhs->type;
  }
  return emit_binary_inst(ctx, op, lhs, rhs, cmp_type);
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
  VReg ne = mir_new_vreg(ctx->mf);
  MirInst inst;
  init_inst(&inst, MIR_OP_NE);
  inst.dst = ne;
  inst.src1 = lhs;
  inst.src2 = rhs;
  inst.type = type;
  mir_emit(ctx->mf, &inst);
  emit_jz(ctx, ne, label);
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

static VReg lower_expr(LowerCtx *ctx, Node *node) {
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
      lower_copy_bytes(ctx, dst_addr, src_addr, get_sizeof(node->lhs->type));
      return src_addr;
    }
    if (node->lhs && node->lhs->type && (node->lhs->type->ty == TY_STRUCT || node->lhs->type->ty == TY_UNION)) {
      VReg dst_addr = lower_addr(ctx, node->lhs);
      VReg src_addr = lower_addr(ctx, node->rhs);
      lower_copy_bytes(ctx, dst_addr, src_addr, get_sizeof(node->lhs->type));
      return src_addr;
    }
    VReg addr = lower_addr(ctx, node->lhs);
    VReg rhs = lower_expr(ctx, node->rhs);
    Type *lhs_type = node->lhs ? node->lhs->type : node->type;
    lower_store_to_addr(ctx, addr, rhs, lhs_type);
    return lower_load_from_addr(ctx, addr, lhs_type);
  }
  case ND_POSTINC:
    return lower_postinc(ctx, node);
  case ND_COMMA:
    (void)lower_expr(ctx, node->lhs);
    return lower_expr(ctx, node->rhs);
  case ND_TYPECAST: {
    VReg src = lower_expr(ctx, node->lhs);
    VReg dst = mir_new_vreg(ctx->mf);
    MirInst inst;
    init_inst(&inst, MIR_OP_CAST);
    inst.dst = dst;
    inst.src1 = src;
    inst.type = node->type;
    mir_emit(ctx->mf, &inst);
    return dst;
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
    VReg src = lower_expr(ctx, node->rhs);
    MirInst inst;
    init_inst(&inst, MIR_OP_RET);
    inst.src1 = src;
    inst.type = node->type;
    mir_emit(ctx->mf, &inst);
    return;
  }
  case ND_IF: {
    VReg cond = lower_expr(ctx, node->cond);
    int l_end = new_label(ctx);
    if (node->els) {
      int l_else = new_label(ctx);
      emit_jz(ctx, cond, l_else);
      lower_stmt(ctx, node->then);
      emit_jmp(ctx, l_end);
      emit_label(ctx, l_else);
      lower_stmt(ctx, node->els);
      emit_label(ctx, l_end);
    } else {
      emit_jz(ctx, cond, l_end);
      lower_stmt(ctx, node->then);
      emit_label(ctx, l_end);
    }
    return;
  }
  case ND_WHILE: {
    int l_begin = new_label(ctx);
    int l_end = new_label(ctx);
    emit_label(ctx, l_begin);
    VReg cond = lower_expr(ctx, node->cond);
    emit_jz(ctx, cond, l_end);

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
    VReg cond = lower_expr(ctx, node->cond);
    emit_jz(ctx, cond, l_end);
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
      VReg cond = lower_expr(ctx, node->cond);
      emit_jz(ctx, cond, l_end);
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
    (void)lower_expr(ctx, node);
    return;
  default:
    lower_error_node("unsupported statement node in lowering", node);
  }
}

static void lower_function(Node *fn_node, MirFunction *mf) {
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
  while (ctx.switch_stack)
    pop_switch_ctx(&ctx, fn_node);
  free_label_map(ctx.label_map);
  ctx.label_map = NULL;
}

static void emit_mir_program(int dump_mir) {
#if LACC_PLATFORM_APPLE
  write_file("  .section __TEXT,__text,regular,pure_instructions\n");
#else
  write_file("  .text\n");
#endif
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    if (code[i]->kind != ND_FUNCDEF)
      continue;
    MirFunction mf;
    lower_function(code[i], &mf);
    if (dump_mir)
      mir_dump(stderr, &mf);
    emit_mir_function(&mf);
    mir_free(&mf);
  }
}

static int should_dump_mir() {
  const char *env = getenv("LACC_DUMP_MIR");
  if (!env || !env[0] || env[0] == '0')
    return false;
  return true;
}

void generate_assembly_optimized() {
  int dump_mir = should_dump_mir();

  write_file(".intel_syntax noprefix\n");
#if !LACC_PLATFORM_APPLE
  write_file("  .section .note.GNU-stack,\"\",@progbits\n");
#endif
  gen_rodata_section();
  gen_data_section();
  emit_mir_program(dump_mir);
}
