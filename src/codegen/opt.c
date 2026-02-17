#include "codegen.h"
#include "diagnostics.h"
#include "mir.h"
#include "runtime.h"

#include <stdbool.h>
#include <stdlib.h>

// ASTをMIRへlowerするときに必要な情報
typedef struct LowerCtx LowerCtx;
struct LowerCtx {
  MirFunction *mf;
  int loop_depth;
  int loop_step_labels[64];
  int loop_end_labels[64];
};

static void lower_error_node(const char *msg, Node *node) {
  if (node)
    error("%s [node kind=%d]", msg, node->kind);
  error("%s", msg);
}

static MirInst new_inst(MirOp op) {
  MirInst inst = {0};
  inst.op = op;
  inst.dst = MIR_INVALID_VREG;
  inst.src1 = MIR_INVALID_VREG;
  inst.src2 = MIR_INVALID_VREG;
  inst.label = MIR_INVALID_LABEL;
  inst.var = NULL;
  inst.call_fn = NULL;
  inst.argc = 0;
  for (int i = 0; i < MAX_FUNC_PARAMS; i++)
    inst.args[i] = MIR_INVALID_VREG;
  return inst;
}

static int new_label(LowerCtx *ctx) { return mir_new_label(ctx->mf); }

static void emit_label(LowerCtx *ctx, int label) {
  MirInst inst = new_inst(MIR_OP_LABEL);
  inst.label = label;
  mir_emit(ctx->mf, inst);
}

static void emit_jmp(LowerCtx *ctx, int label) {
  MirInst inst = new_inst(MIR_OP_JMP);
  inst.label = label;
  mir_emit(ctx->mf, inst);
}

static void emit_jz(LowerCtx *ctx, VReg cond, int label) {
  MirInst inst = new_inst(MIR_OP_JZ);
  inst.src1 = cond;
  inst.label = label;
  mir_emit(ctx->mf, inst);
}

static VReg lower_expr(LowerCtx *ctx, Node *node);
static void lower_stmt(LowerCtx *ctx, Node *node);

static VReg lower_imm(LowerCtx *ctx, long imm, Type *type) {
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst inst = new_inst(MIR_OP_IMM);
  inst.dst = dst;
  inst.imm = imm;
  inst.type = type;
  mir_emit(ctx->mf, inst);
  return dst;
}

static void emit_mov(LowerCtx *ctx, VReg dst, VReg src, Type *type) {
  MirInst inst = new_inst(MIR_OP_MOV);
  inst.dst = dst;
  inst.src1 = src;
  inst.type = type;
  mir_emit(ctx->mf, inst);
}

static VReg lower_not(LowerCtx *ctx, Node *node) {
  VReg src = lower_expr(ctx, node->lhs);
  VReg zero = lower_imm(ctx, 0, node->type);
  VReg dst = mir_new_vreg(ctx->mf);
  MirInst eq = new_inst(MIR_OP_EQ);
  eq.dst = dst;
  eq.src1 = src;
  eq.src2 = zero;
  eq.type = node->type;
  mir_emit(ctx->mf, eq);
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
  MirInst inst = new_inst(MIR_OP_ADDR_SYMBOL);
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
  mir_emit(ctx->mf, inst);
  return dst;
}

static VReg lower_load_from_addr(LowerCtx *ctx, VReg addr, Type *type) {
  if (type && type->ty == TY_ARR)
    return addr;

  VReg dst = mir_new_vreg(ctx->mf);
  MirInst load = new_inst(MIR_OP_LOAD);
  load.dst = dst;
  load.src1 = addr;
  load.type = type;
  mir_emit(ctx->mf, load);
  return dst;
}

static void lower_store_to_addr(LowerCtx *ctx, VReg addr, VReg value, Type *type) {
  MirInst store = new_inst(MIR_OP_STORE);
  store.src1 = addr;
  store.src2 = value;
  store.type = type;
  mir_emit(ctx->mf, store);
}

static VReg lower_binary(LowerCtx *ctx, Node *node, MirOp op) {
  VReg lhs = lower_expr(ctx, node->lhs);
  VReg rhs = lower_expr(ctx, node->rhs);
  VReg dst = mir_new_vreg(ctx->mf);

  MirInst inst = new_inst(op);
  inst.dst = dst;
  inst.src1 = lhs;
  inst.src2 = rhs;
  inst.type = node->type;
  mir_emit(ctx->mf, inst);
  return dst;
}

static VReg lower_call(LowerCtx *ctx, Node *node) {
  MirInst call = new_inst(MIR_OP_CALL);

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

  mir_emit(ctx->mf, call);
  return call.dst;
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
    MirInst inst = new_inst(MIR_OP_ADDR_FUNC);
    inst.dst = dst;
    inst.call_fn = node->fn;
    inst.type = node->type;
    mir_emit(ctx->mf, inst);
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
    return lower_binary(ctx, node, MIR_OP_LT);
  case ND_LE:
    return lower_binary(ctx, node, MIR_OP_LE);
  case ND_AND:
    return lower_logical_and(ctx, node);
  case ND_OR:
    return lower_logical_or(ctx, node);
  case ND_NOT:
    return lower_not(ctx, node);
  case ND_ASSIGN: {
    VReg addr = lower_addr(ctx, node->lhs);
    VReg rhs = lower_expr(ctx, node->rhs);
    lower_store_to_addr(ctx, addr, rhs, node->lhs ? node->lhs->type : node->type);
    return rhs;
  }
  case ND_COMMA:
    (void)lower_expr(ctx, node->lhs);
    return lower_expr(ctx, node->rhs);
  case ND_TYPECAST: {
    VReg src = lower_expr(ctx, node->lhs);
    VReg dst = mir_new_vreg(ctx->mf);
    MirInst inst = new_inst(MIR_OP_CAST);
    inst.dst = dst;
    inst.src1 = src;
    inst.type = node->type;
    mir_emit(ctx->mf, inst);
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
    MirInst inst = new_inst(MIR_OP_RET);
    inst.src1 = src;
    inst.type = node->type;
    mir_emit(ctx->mf, inst);
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

    if (ctx->loop_depth >= 64)
      lower_error_node("loop nesting too deep", node);
    ctx->loop_step_labels[ctx->loop_depth] = l_begin;
    ctx->loop_end_labels[ctx->loop_depth] = l_end;
    ctx->loop_depth++;
    lower_stmt(ctx, node->then);
    ctx->loop_depth--;

    emit_jmp(ctx, l_begin);
    emit_label(ctx, l_end);
    return;
  }
  case ND_DOWHILE: {
    int l_begin = new_label(ctx);
    int l_step = new_label(ctx);
    int l_end = new_label(ctx);
    emit_label(ctx, l_begin);

    if (ctx->loop_depth >= 64)
      lower_error_node("loop nesting too deep", node);
    ctx->loop_step_labels[ctx->loop_depth] = l_step;
    ctx->loop_end_labels[ctx->loop_depth] = l_end;
    ctx->loop_depth++;
    lower_stmt(ctx, node->then);
    ctx->loop_depth--;

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

    if (ctx->loop_depth >= 64)
      lower_error_node("loop nesting too deep", node);
    ctx->loop_step_labels[ctx->loop_depth] = l_step;
    ctx->loop_end_labels[ctx->loop_depth] = l_end;
    ctx->loop_depth++;
    lower_stmt(ctx, node->then);
    ctx->loop_depth--;

    emit_label(ctx, l_step);
    if (node->step && node->step->kind != ND_NONE)
      lower_stmt(ctx, node->step);
    emit_jmp(ctx, l_begin);
    emit_label(ctx, l_end);
    return;
  }
  case ND_BREAK:
    if (ctx->loop_depth <= 0)
      lower_error_node("break outside of loop in lowering", node);
    emit_jmp(ctx, ctx->loop_end_labels[ctx->loop_depth - 1]);
    return;
  case ND_CONTINUE:
    if (ctx->loop_depth <= 0)
      lower_error_node("continue outside of loop in lowering", node);
    emit_jmp(ctx, ctx->loop_step_labels[ctx->loop_depth - 1]);
    return;
  case ND_VARDEC:
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
  LowerCtx ctx = {0};
  ctx.mf = mf;
  lower_stmt(&ctx, fn_node->lhs);
}

static int should_dump_mir() {
  const char *env = getenv("LACC_DUMP_MIR");
  if (!env || !env[0] || env[0] == '0')
    return false;
  return true;
}

void generate_assembly_optimized() {
  int dump_mir = should_dump_mir();

  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    if (code[i]->kind != ND_FUNCDEF)
      continue;

    MirFunction mf;
    lower_function(code[i], &mf);

    if (dump_mir)
      mir_dump(stderr, &mf);
    mir_free(&mf);
  }

  // Lowering is connected, but asm emission is still the existing backend.
  generate_assembly();
}
