#include "diagnostics.h"
#include "opt_internal.h"
#include "opt_regalloc.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

enum {
  MIR_CALL_ARG_REGS = 6,
};

static const char *mir_arg_regs1[MIR_CALL_ARG_REGS] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static const char *mir_arg_regs2[MIR_CALL_ARG_REGS] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static const char *mir_arg_regs4[MIR_CALL_ARG_REGS] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static const char *mir_arg_regs8[MIR_CALL_ARG_REGS] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

typedef struct MirAsmCtx MirAsmCtx;
struct MirAsmCtx {
  const MirFunction *mf;
  const RegAllocResult *ra;
  const char *epilogue_label;
  int spill_base;
  int save_base;
  int save_slot[RA_PREG_COUNT];
  int frame_size;
};

static int align_up(int n, int align) {
  if (align <= 0)
    return n;
  return ((n + align - 1) / align) * align;
} 

static int count_mask_bits(unsigned mask) {
  int c = 0;
  while (mask) {
    c += (mask & 1u);
    mask >>= 1;
  }
  return c;
}

static int align_frame_size_for_calls(int size) {
  int want_mod = 0;
  int aligned = align_up(size, 16);
  if ((aligned & 15) != want_mod)
    aligned += 8;
  return aligned;
}

static const RaVRegLoc *vreg_loc(const MirAsmCtx *ctx, VReg vreg) {
  if (!ctx || !ctx->ra || !ctx->ra->locs || vreg < 0 || vreg >= ctx->ra->vreg_count)
    error("invalid virtual register [in vreg_loc]");
  return &ctx->ra->locs[vreg];
}

static int spill_slot_offset(const MirAsmCtx *ctx, int slot) {
  if (!ctx || slot < 0)
    error("invalid spill slot [in spill_slot_offset]");
  return ctx->spill_base + (slot + 1) * 8;
}

static void load_vreg_to_reg(const MirAsmCtx *ctx, VReg vreg, const char *reg64) {
  const RaVRegLoc *loc = vreg_loc(ctx, vreg);
  if (loc->kind == RA_LOC_REG) {
    const char *src = ra_preg64(loc->reg);
    if (strcmp(src, reg64))
      write_file("  mov %s, %s\n", reg64, src);
    return;
  }
  if (loc->kind == RA_LOC_STACK) {
    int off = spill_slot_offset(ctx, loc->stack_slot);
    write_file("  mov %s, QWORD PTR [rbp - %d]\n", reg64, off);
    return;
  }
  error("invalid vreg location [in load_vreg_to_reg]");
}

static void store_reg_to_vreg(const MirAsmCtx *ctx, VReg vreg, const char *reg64) {
  const RaVRegLoc *loc = vreg_loc(ctx, vreg);
  if (loc->kind == RA_LOC_REG) {
    const char *dst = ra_preg64(loc->reg);
    if (strcmp(dst, reg64))
      write_file("  mov %s, %s\n", dst, reg64);
    return;
  }
  if (loc->kind == RA_LOC_STACK) {
    int off = spill_slot_offset(ctx, loc->stack_slot);
    write_file("  mov QWORD PTR [rbp - %d], %s\n", off, reg64);
    return;
  }
  error("invalid vreg location [in store_reg_to_vreg]");
}

static void load_rax_from_local(int offset, Type *type) {
  if (!type)
    error("missing type [in load_rax_from_local]");
  switch (type->ty) {
  case TY_BOOL:
    if (type->is_unsigned)
      write_file("  movzx rax, BYTE PTR [rbp - %d]\n", offset);
    else
      write_file("  movsx rax, BYTE PTR [rbp - %d]\n", offset);
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov eax, DWORD PTR [rbp - %d]\n", offset);
    else
      write_file("  movsxd rax, DWORD PTR [rbp - %d]\n", offset);
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx rax, BYTE PTR [rbp - %d]\n", offset);
    else
      write_file("  movsx rax, BYTE PTR [rbp - %d]\n", offset);
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx rax, WORD PTR [rbp - %d]\n", offset);
    else
      write_file("  movsx rax, WORD PTR [rbp - %d]\n", offset);
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov rax, QWORD PTR [rbp - %d]\n", offset);
    break;
  default:
    error("unsupported type in MIR local load [ty=%d]", type->ty);
  }
}

static void store_reg_to_local(int offset, Type *type, const char *reg1, const char *reg2, const char *reg4,
                               const char *reg8) {
  if (!type)
    error("missing type [in store_reg_to_local]");
  switch (type->ty) {
  case TY_BOOL:
  case TY_CHAR:
    write_file("  mov BYTE PTR [rbp - %d], %s\n", offset, reg1);
    break;
  case TY_SHORT:
    write_file("  mov WORD PTR [rbp - %d], %s\n", offset, reg2);
    break;
  case TY_INT:
    write_file("  mov DWORD PTR [rbp - %d], %s\n", offset, reg4);
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov QWORD PTR [rbp - %d], %s\n", offset, reg8);
    break;
  default:
    error("unsupported type in MIR local store [ty=%d]", type->ty);
  }
}

static void load_rax_from_ptr(Type *type) {
  if (!type)
    error("missing type [in load_rax_from_ptr]");
  switch (type->ty) {
  case TY_BOOL:
    if (type->is_unsigned)
      write_file("  movzx rax, BYTE PTR [rax]\n");
    else
      write_file("  movsx rax, BYTE PTR [rax]\n");
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov eax, DWORD PTR [rax]\n");
    else
      write_file("  movsxd rax, DWORD PTR [rax]\n");
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx rax, BYTE PTR [rax]\n");
    else
      write_file("  movsx rax, BYTE PTR [rax]\n");
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx rax, WORD PTR [rax]\n");
    else
      write_file("  movsx rax, WORD PTR [rax]\n");
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov rax, QWORD PTR [rax]\n");
    break;
  default:
    error("unsupported type in MIR load [ty=%d]", type->ty);
  }
}

static void store_rdi_to_ptr(Type *type) {
  if (!type)
    error("missing type [in store_rdi_to_ptr]");
  switch (type->ty) {
  case TY_BOOL:
    write_file("  cmp rdi, 0\n");
    write_file("  setne dil\n");
    write_file("  mov BYTE PTR [rax], dil\n");
    break;
  case TY_INT:
    write_file("  mov DWORD PTR [rax], edi\n");
    break;
  case TY_CHAR:
    write_file("  mov BYTE PTR [rax], dil\n");
    break;
  case TY_SHORT:
    write_file("  mov WORD PTR [rax], di\n");
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov QWORD PTR [rax], rdi\n");
    break;
  default:
    error("unsupported type in MIR store [ty=%d]", type->ty);
  }
}

static void emit_cast_rax(Type *type) {
  if (!type)
    return;
  switch (type->ty) {
  case TY_BOOL:
    write_file("  cmp rax, 0\n");
    write_file("  setne al\n");
    write_file("  movzx eax, al\n");
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx eax, al\n");
    else
      write_file("  movsx eax, al\n");
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx eax, ax\n");
    else
      write_file("  movsx eax, ax\n");
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov eax, eax\n");
    else
      write_file("  movsxd rax, eax\n");
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
    break;
  default:
    error("unsupported type in MIR cast [ty=%d]", type->ty);
  }
}

static void emit_mir_label(const MirFunction *mf, int label) {
  write_file(".Lmir.%.*s.%d:\n", mf->fn->len, mf->fn->name, label);
}

static void emit_mir_jmp(const MirFunction *mf, int label) {
  write_file("  jmp .Lmir.%.*s.%d\n", mf->fn->len, mf->fn->name, label);
}

static void normalize_cmp_operands(Type *type) {
  if (!type)
    return;
  int sz = type_size(type);
  if (type->is_unsigned) {
    if (sz == 1) {
      write_file("  movzx eax, al\n");
      write_file("  movzx edi, dil\n");
    } else if (sz == 2) {
      write_file("  movzx eax, ax\n");
      write_file("  movzx edi, di\n");
    } else if (sz == 4) {
      write_file("  mov eax, eax\n");
      write_file("  mov edi, edi\n");
    }
  } else {
    if (sz == 1) {
      write_file("  movsx eax, al\n");
      write_file("  movsx edi, dil\n");
    } else if (sz == 2) {
      write_file("  movsx eax, ax\n");
      write_file("  movsx edi, di\n");
    } else if (sz == 4) {
      write_file("  movsxd rax, eax\n");
      write_file("  movsxd rdi, edi\n");
    }
  }
}

static void emit_mir_inst(MirAsmCtx *ctx, const MirInst *in) {
  if (!ctx || !ctx->mf || !ctx->mf->fn || !in)
    error("invalid MIR emit context");

  switch (in->op) {
  case MIR_OP_NOP:
    write_file("  nop\n");
    return;
  case MIR_OP_IMM:
    write_file("  mov rax, %ld\n", in->imm);
    emit_cast_rax(in->type);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_MOV:
    load_vreg_to_reg(ctx, in->src1, "rax");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_LOAD_LOCAL:
    load_rax_from_local(in->offset, in->type);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_STORE_LOCAL:
    load_vreg_to_reg(ctx, in->src1, "rdi");
    store_reg_to_local(in->offset, in->type, "dil", "di", "edi", "rdi");
    return;
  case MIR_OP_ADDR_LOCAL:
    write_file("  lea rax, [rbp - %d]\n", in->offset);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADDR_SYMBOL:
    if (!in->var)
      error("missing symbol in MIR_OP_ADDR_SYMBOL");
    if (in->var->is_static && in->var->block > 0) {
      write_file("  lea rax, %s%.*s.%d[rip]\n", ASM_PREFIX, in->var->len, in->var->name, in->var->block);
    } else if (in->var->is_static) {
      write_file("  lea rax, " ASM_SYM_FMT "[rip]\n", ASM_SYM_ARGS(in->var->len, in->var->name));
    } else {
      write_file("  mov rax, QWORD PTR [rip + " ASM_SYM_FMT "@GOTPCREL]\n", ASM_SYM_ARGS(in->var->len, in->var->name));
    }
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADDR_FUNC:
    if (!in->call_fn)
      error("missing function in MIR_OP_ADDR_FUNC");
    write_file("  lea rax, " ASM_SYM_FMT "[rip]\n", ASM_SYM_ARGS(in->call_fn->len, in->call_fn->name));
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADDR_STRING:
    write_file("  lea rax, [rip + .L.str%d]\n", in->offset);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADDR_ARRAY:
    write_file("  lea rax, [rip + .L.arr%d]\n", in->offset);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADDR_STRUCT_LITERAL:
    write_file("  lea rax, [rip + .L.struct%d]\n", in->offset);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_LOAD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_rax_from_ptr(in->type);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_STORE:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    store_rdi_to_ptr(in->type);
    return;
  case MIR_OP_CAST:
    load_vreg_to_reg(ctx, in->src1, "rax");
    emit_cast_rax(in->type);
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_ADD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  add rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_SUB:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  sub rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_MUL:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  imul rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_SDIV:
  case MIR_OP_SMOD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  cqo\n");
    write_file("  idiv rdi\n");
    if (in->op == MIR_OP_SMOD)
      write_file("  mov rax, rdx\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_UDIV:
  case MIR_OP_UMOD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  xor rdx, rdx\n");
    write_file("  div rdi\n");
    if (in->op == MIR_OP_UMOD)
      write_file("  mov rax, rdx\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_EQ:
  case MIR_OP_NE:
  case MIR_OP_LT:
  case MIR_OP_LE:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    normalize_cmp_operands(in->type);
    write_file("  cmp rax, rdi\n");
    if (in->op == MIR_OP_EQ)
      write_file("  sete al\n");
    else if (in->op == MIR_OP_NE)
      write_file("  setne al\n");
    else if (in->op == MIR_OP_LT) {
      if (in->type && in->type->is_unsigned)
        write_file("  setb al\n");
      else
        write_file("  setl al\n");
    } else {
      if (in->type && in->type->is_unsigned)
        write_file("  setbe al\n");
      else
        write_file("  setle al\n");
    }
    write_file("  movzx eax, al\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_BITAND:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  and rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_BITOR:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  or rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_BITXOR:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  xor rax, rdi\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_SHL:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  mov rcx, rdi\n");
    if (in->type && type_size(in->type) == 4)
      write_file("  shl eax, cl\n");
    else
      write_file("  shl rax, cl\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_SHR:
    load_vreg_to_reg(ctx, in->src1, "rax");
    load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  mov rcx, rdi\n");
    if (in->type && type_size(in->type) == 4) {
      if (in->type->is_unsigned)
        write_file("  shr eax, cl\n");
      else
        write_file("  sar eax, cl\n");
    } else {
      if (in->type && in->type->is_unsigned)
        write_file("  shr rax, cl\n");
      else
        write_file("  sar rax, cl\n");
    }
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_BITNOT:
    load_vreg_to_reg(ctx, in->src1, "rax");
    write_file("  not rax\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_LABEL:
    emit_mir_label(ctx->mf, in->label);
    return;
  case MIR_OP_JMP:
    emit_mir_jmp(ctx->mf, in->label);
    return;
  case MIR_OP_JZ:
    load_vreg_to_reg(ctx, in->src1, "rax");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lmir.%.*s.%d\n", ctx->mf->fn->len, ctx->mf->fn->name, in->label);
    return;
  case MIR_OP_CALL: {
    int reg_argc = in->argc < MIR_CALL_ARG_REGS ? in->argc : MIR_CALL_ARG_REGS;
    int stack_argc = in->argc - reg_argc;
    int align_pad = stack_argc & 1;

    // Prologue keeps rsp 16-byte aligned. For odd stack-arg count, reserve 8-byte pad first.
    if (align_pad)
      write_file("  sub rsp, 8\n");

    for (int a = in->argc - 1; a >= 0; a--) {
      load_vreg_to_reg(ctx, in->args[a], "rax");
      write_file("  push rax\n");
    }

    if (!in->call_fn) {
      load_vreg_to_reg(ctx, in->src1, "r10");
    }

    for (int a = 0; a < reg_argc; a++) {
      write_file("  pop rax\n");
      write_file("  mov %s, rax\n", mir_arg_regs8[a]);
    }

    write_file("  mov rax, 0\n");
    if (in->call_fn) {
      write_file("  call " ASM_SYM_FMT "\n", ASM_SYM_ARGS(in->call_fn->len, in->call_fn->name));
    } else {
      write_file("  call r10\n");
    }

    if (stack_argc > 0 || align_pad)
      write_file("  add rsp, %d\n", (stack_argc * 8) + (align_pad ? 8 : 0));
    if (in->dst != MIR_INVALID_VREG)
      store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  }
  case MIR_OP_RET:
    if (in->src1 != MIR_INVALID_VREG)
      load_vreg_to_reg(ctx, in->src1, "rax");
    else
      write_file("  mov rax, 0\n");
    if (in->type && in->type->ty == TY_BOOL) {
      write_file("  cmp rax, 0\n");
      write_file("  setne al\n");
      write_file("  movzx eax, al\n");
    }
    write_file("  jmp %s\n", ctx->epilogue_label);
    return;
  }

  error("unsupported MIR op in asm emitter [op=%d]", in->op);
}

void emit_mir_function(const MirFunction *mf) {
  if (!mf || !mf->fn)
    error("invalid MIR function in asm emitter");

  RegAllocResult ra = {0};
  regalloc_run(mf, &ra);

  MirAsmCtx ctx = {0};
  ctx.mf = mf;
  ctx.ra = &ra;
  ctx.spill_base = align_up(mf->fn->offset, 8);
  int save_count = count_mask_bits(ra.used_reg_mask);
  ctx.save_base = ctx.spill_base + (ra.spill_count * 8);
  int frame_payload = ctx.save_base + (save_count * 8);
  ctx.frame_size = align_frame_size_for_calls(frame_payload);
  for (int p = 0; p < RA_PREG_COUNT; p++)
    ctx.save_slot[p] = -1;

  int is_local_symbol = mf->fn->is_static || mf->fn->is_inline;
#if !LACC_PLATFORM_APPLE
  if (is_local_symbol) {
    write_file("  .local " ASM_SYM_FMT "\n", ASM_SYM_ARGS(mf->fn->len, mf->fn->name));
  } else {
    write_file("  .globl " ASM_SYM_FMT "\n", ASM_SYM_ARGS(mf->fn->len, mf->fn->name));
  }
#else
  if (!is_local_symbol)
    write_file("  .globl " ASM_SYM_FMT "\n", ASM_SYM_ARGS(mf->fn->len, mf->fn->name));
#endif
  write_file("  .p2align 4\n");
  write_file(ASM_SYM_FMT ":\n", ASM_SYM_ARGS(mf->fn->len, mf->fn->name));
  write_file("  push rbp\n");
  write_file("  mov rbp, rsp\n");
  if (ctx.frame_size > 0)
    write_file("  sub rsp, %d\n", ctx.frame_size);
  int save_idx = 0;
  for (int p = 0; p < RA_PREG_COUNT; p++) {
    if (!(ra.used_reg_mask & (1u << p)))
      continue;
    int off = ctx.save_base + ((save_idx + 1) * 8);
    ctx.save_slot[p] = off;
    write_file("  mov QWORD PTR [rbp - %d], %s\n", off, ra_preg64(p));
    save_idx++;
  }

  char epilogue_label[256];
  int n = snprintf(epilogue_label, sizeof(epilogue_label), ".Lmir.epilogue.%.*s", mf->fn->len, mf->fn->name);
  if (n <= 0 || n >= (int)sizeof(epilogue_label))
    error("epilogue label too long in MIR asm emitter");
  ctx.epilogue_label = epilogue_label;

  for (int i = 0; i < mf->param_count; i++) {
    int off = mf->param_offsets[i];
    Type *ty = mf->param_types[i];
    if (i < MIR_CALL_ARG_REGS) {
      store_reg_to_local(off, ty, mir_arg_regs1[i], mir_arg_regs2[i], mir_arg_regs4[i], mir_arg_regs8[i]);
    } else {
      int stack_arg_off = 16 + (i - MIR_CALL_ARG_REGS) * 8;
      write_file("  mov r11, QWORD PTR [rbp + %d]\n", stack_arg_off);
      store_reg_to_local(off, ty, "r11b", "r11w", "r11d", "r11");
    }
  }

  for (int i = 0; i < mf->inst_len; i++)
    emit_mir_inst(&ctx, &mf->insts[i]);

  if (mf->fn->type->return_type->ty == TY_VOID || !strncmp(mf->fn->name, "main", 4)) {
    write_file("  mov rax, 0\n");
    write_file("  jmp %s\n", ctx.epilogue_label);
  }

  write_file("%s:\n", ctx.epilogue_label);
  for (int p = RA_PREG_COUNT - 1; p >= 0; p--) {
    if (ctx.save_slot[p] >= 0)
      write_file("  mov %s, QWORD PTR [rbp - %d]\n", ra_preg64(p), ctx.save_slot[p]);
  }
  write_file("  mov rsp, rbp\n");
  write_file("  pop rbp\n");
  write_file("  ret\n");

  regalloc_free(&ra);
}
