#include "codegen.h"
#include "mir.h"
#include "runtime.h"

#include <stdbool.h>
#include <stdlib.h>

static MirInst new_inst(MirOp op) {
  MirInst inst = {0};
  inst.op = op;
  inst.dst = MIR_INVALID_VREG;
  inst.src1 = MIR_INVALID_VREG;
  inst.src2 = MIR_INVALID_VREG;
  return inst;
}

static bool lower_expr(Node *node, MirFunction *mf, VReg *out) {
  if (!node || !mf || !out)
    return false;

  switch (node->kind) {
  case ND_NUM: {
    VReg dst = mir_new_vreg(mf);
    MirInst inst = new_inst(MIR_OP_IMM);
    inst.dst = dst;
    inst.imm = node->val;
    inst.type = node->type;
    mir_emit(mf, inst);
    *out = dst;
    return true;
  }
  case ND_LVAR: {
    if (!node->var || node->var->is_static)
      return false;
    VReg dst = mir_new_vreg(mf);
    MirInst inst = new_inst(MIR_OP_LOAD_LOCAL);
    inst.dst = dst;
    inst.offset = node->var->offset;
    inst.type = node->type;
    mir_emit(mf, inst);
    *out = dst;
    return true;
  }
  case ND_ADD: {
    VReg lhs = MIR_INVALID_VREG;
    VReg rhs = MIR_INVALID_VREG;
    if (!lower_expr(node->lhs, mf, &lhs))
      return false;
    if (!lower_expr(node->rhs, mf, &rhs))
      return false;
    VReg dst = mir_new_vreg(mf);
    MirInst inst = new_inst(MIR_OP_ADD);
    inst.dst = dst;
    inst.src1 = lhs;
    inst.src2 = rhs;
    inst.type = node->type;
    mir_emit(mf, inst);
    *out = dst;
    return true;
  }
  default:
    return false;
  }
}

static bool lower_stmt(Node *node, MirFunction *mf) {
  if (!node || !mf)
    return false;

  switch (node->kind) {
  case ND_BLOCK:
    for (int i = 0; node->body && node->body[i] && node->body[i]->kind != ND_NONE; i++) {
      if (!lower_stmt(node->body[i], mf))
        return false;
    }
    return true;
  case ND_RETURN: {
    VReg src = MIR_INVALID_VREG;
    if (!lower_expr(node->rhs, mf, &src))
      return false;
    MirInst inst = new_inst(MIR_OP_RET);
    inst.src1 = src;
    inst.type = node->type;
    mir_emit(mf, inst);
    return true;
  }
  default:
    return false;
  }
}

static bool lower_function(Node *fn_node, MirFunction *mf) {
  if (!fn_node || fn_node->kind != ND_FUNCDEF || !fn_node->fn || !fn_node->lhs || !mf)
    return false;

  mir_init(mf, fn_node->fn);
  return lower_stmt(fn_node->lhs, mf);
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
    if (!lower_function(code[i], &mf)) {
      // Step 4: Unsupported nodes still use the existing backend.
      generate_assembly();
      return;
    }

    if (dump_mir)
      mir_dump(stderr, &mf);
    mir_free(&mf);
  }

  // Step 4: lowering is connected, but asm emission is still the existing backend.
  generate_assembly();
}
