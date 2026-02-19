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

typedef struct {
  const char *r8;
  const char *r16;
  const char *r32;
  const char *r64;
} RegNames;

static const RegNames k_rn_rax = {"al", "ax", "eax", "rax"};
static const RegNames k_rn_rbx = {"bl", "bx", "ebx", "rbx"};
static const RegNames k_rn_rcx = {"cl", "cx", "ecx", "rcx"};
static const RegNames k_rn_rdx = {"dl", "dx", "edx", "rdx"};
static const RegNames k_rn_rsi = {"sil", "si", "esi", "rsi"};
static const RegNames k_rn_rdi = {"dil", "di", "edi", "rdi"};
static const RegNames k_rn_r8 = {"r8b", "r8w", "r8d", "r8"};
static const RegNames k_rn_r9 = {"r9b", "r9w", "r9d", "r9"};
static const RegNames k_rn_r10 = {"r10b", "r10w", "r10d", "r10"};
static const RegNames k_rn_r11 = {"r11b", "r11w", "r11d", "r11"};
static const RegNames k_rn_r12 = {"r12b", "r12w", "r12d", "r12"};
static const RegNames k_rn_r13 = {"r13b", "r13w", "r13d", "r13"};
static const RegNames k_rn_r14 = {"r14b", "r14w", "r14d", "r14"};
static const RegNames k_rn_r15 = {"r15b", "r15w", "r15d", "r15"};

static const RegNames *reg_names_for64(const char *r64) {
  if (!r64)
    error("missing register name [in reg_names_for64]");
  if (!strcmp(r64, "rax"))
    return &k_rn_rax;
  if (!strcmp(r64, "rbx"))
    return &k_rn_rbx;
  if (!strcmp(r64, "rcx"))
    return &k_rn_rcx;
  if (!strcmp(r64, "rdx"))
    return &k_rn_rdx;
  if (!strcmp(r64, "rsi"))
    return &k_rn_rsi;
  if (!strcmp(r64, "rdi"))
    return &k_rn_rdi;
  if (!strcmp(r64, "r8"))
    return &k_rn_r8;
  if (!strcmp(r64, "r9"))
    return &k_rn_r9;
  if (!strcmp(r64, "r10"))
    return &k_rn_r10;
  if (!strcmp(r64, "r11"))
    return &k_rn_r11;
  if (!strcmp(r64, "r12"))
    return &k_rn_r12;
  if (!strcmp(r64, "r13"))
    return &k_rn_r13;
  if (!strcmp(r64, "r14"))
    return &k_rn_r14;
  if (!strcmp(r64, "r15"))
    return &k_rn_r15;
  error("unsupported register name [in reg_names_for64: %s]", r64);
  return NULL;
}

static const char *reg8_name(const char *r64) { return reg_names_for64(r64)->r8; }
static const char *reg16_name(const char *r64) { return reg_names_for64(r64)->r16; }
static const char *reg32_name(const char *r64) { return reg_names_for64(r64)->r32; }

static const char *vreg_assigned_reg64(const MirAsmCtx *ctx, VReg vreg) {
  const RaVRegLoc *loc = vreg_loc(ctx, vreg);
  if (loc->kind == RA_LOC_REG)
    return ra_preg64(loc->reg);
  if (loc->kind == RA_LOC_STACK)
    return NULL;
  error("invalid vreg location [in vreg_assigned_reg64]");
  return NULL;
}

static int vreg_stack_offset_or_neg1(const MirAsmCtx *ctx, VReg vreg) {
  const RaVRegLoc *loc = vreg_loc(ctx, vreg);
  if (loc->kind == RA_LOC_STACK)
    return spill_slot_offset(ctx, loc->stack_slot);
  if (loc->kind == RA_LOC_REG)
    return -1;
  error("invalid vreg location [in vreg_stack_offset_or_neg1]");
  return -1;
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

static void load_reg_from_local(int offset, Type *type, const char *dst_reg64) {
  if (!type)
    error("missing type [in load_reg_from_local]");
  if (!dst_reg64)
    error("missing dst reg names [in load_reg_from_local]");
  switch (type->ty) {
  case TY_BOOL:
    if (type->is_unsigned)
      write_file("  movzx %s, BYTE PTR [rbp - %d]\n", dst_reg64, offset);
    else
      write_file("  movsx %s, BYTE PTR [rbp - %d]\n", dst_reg64, offset);
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov %s, DWORD PTR [rbp - %d]\n", reg32_name(dst_reg64), offset);
    else
      write_file("  movsxd %s, DWORD PTR [rbp - %d]\n", dst_reg64, offset);
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx %s, BYTE PTR [rbp - %d]\n", dst_reg64, offset);
    else
      write_file("  movsx %s, BYTE PTR [rbp - %d]\n", dst_reg64, offset);
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx %s, WORD PTR [rbp - %d]\n", dst_reg64, offset);
    else
      write_file("  movsx %s, WORD PTR [rbp - %d]\n", dst_reg64, offset);
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov %s, QWORD PTR [rbp - %d]\n", dst_reg64, offset);
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

static void store_reg_to_local_by64(int offset, Type *type, const char *reg64) {
  store_reg_to_local(offset, type, reg8_name(reg64), reg16_name(reg64), reg32_name(reg64), reg64);
}

static void load_reg_from_ptr(Type *type, const char *dst_reg64, const char *ptr_reg64) {
  if (!ptr_reg64)
    error("missing pointer register [in load_reg_from_ptr]");
  if (!type)
    error("missing type [in load_reg_from_ptr]");
  switch (type->ty) {
  case TY_BOOL:
    if (type->is_unsigned)
      write_file("  movzx %s, BYTE PTR [%s]\n", dst_reg64, ptr_reg64);
    else
      write_file("  movsx %s, BYTE PTR [%s]\n", dst_reg64, ptr_reg64);
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov %s, DWORD PTR [%s]\n", reg32_name(dst_reg64), ptr_reg64);
    else
      write_file("  movsxd %s, DWORD PTR [%s]\n", dst_reg64, ptr_reg64);
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx %s, BYTE PTR [%s]\n", dst_reg64, ptr_reg64);
    else
      write_file("  movsx %s, BYTE PTR [%s]\n", dst_reg64, ptr_reg64);
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx %s, WORD PTR [%s]\n", dst_reg64, ptr_reg64);
    else
      write_file("  movsx %s, WORD PTR [%s]\n", dst_reg64, ptr_reg64);
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov %s, QWORD PTR [%s]\n", dst_reg64, ptr_reg64);
    break;
  default:
    error("unsupported type in MIR load [ty=%d]", type->ty);
  }
}

static void store_reg_to_ptr(Type *type, const char *ptr_reg64, const char *src_reg64) {
  if (!ptr_reg64)
    error("missing pointer register [in store_reg_to_ptr]");
  if (!type)
    error("missing type [in store_reg_to_ptr]");
  switch (type->ty) {
  case TY_BOOL:
    write_file("  cmp %s, 0\n", src_reg64);
    write_file("  setne %s\n", reg8_name(src_reg64));
    write_file("  mov BYTE PTR [%s], %s\n", ptr_reg64, reg8_name(src_reg64));
    break;
  case TY_INT:
    write_file("  mov DWORD PTR [%s], %s\n", ptr_reg64, reg32_name(src_reg64));
    break;
  case TY_CHAR:
    write_file("  mov BYTE PTR [%s], %s\n", ptr_reg64, reg8_name(src_reg64));
    break;
  case TY_SHORT:
    write_file("  mov WORD PTR [%s], %s\n", ptr_reg64, reg16_name(src_reg64));
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov QWORD PTR [%s], %s\n", ptr_reg64, src_reg64);
    break;
  default:
    error("unsupported type in MIR store [ty=%d]", type->ty);
  }
}

static void emit_cast_reg(Type *type, const char *reg64) {
  if (!type)
    return;
  switch (type->ty) {
  case TY_BOOL:
    write_file("  cmp %s, 0\n", reg64);
    write_file("  setne %s\n", reg8_name(reg64));
    write_file("  movzx %s, %s\n", reg32_name(reg64), reg8_name(reg64));
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx %s, %s\n", reg32_name(reg64), reg8_name(reg64));
    else
      write_file("  movsx %s, %s\n", reg32_name(reg64), reg8_name(reg64));
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx %s, %s\n", reg32_name(reg64), reg16_name(reg64));
    else
      write_file("  movsx %s, %s\n", reg32_name(reg64), reg16_name(reg64));
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov %s, %s\n", reg32_name(reg64), reg32_name(reg64));
    else
      write_file("  movsxd %s, %s\n", reg64, reg32_name(reg64));
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

static void normalize_cmp_operands(Type *type, const char *lhs_reg64, const char *rhs_reg64) {
  if (!type)
    return;
  int sz = type_size(type);
  if (type->is_unsigned) {
    if (sz == 1) {
      write_file("  movzx %s, %s\n", reg32_name(lhs_reg64), reg8_name(lhs_reg64));
      write_file("  movzx %s, %s\n", reg32_name(rhs_reg64), reg8_name(rhs_reg64));
    } else if (sz == 2) {
      write_file("  movzx %s, %s\n", reg32_name(lhs_reg64), reg16_name(lhs_reg64));
      write_file("  movzx %s, %s\n", reg32_name(rhs_reg64), reg16_name(rhs_reg64));
    } else if (sz == 4) {
      write_file("  mov %s, %s\n", reg32_name(lhs_reg64), reg32_name(lhs_reg64));
      write_file("  mov %s, %s\n", reg32_name(rhs_reg64), reg32_name(rhs_reg64));
    }
  } else {
    if (sz == 1) {
      write_file("  movsx %s, %s\n", reg32_name(lhs_reg64), reg8_name(lhs_reg64));
      write_file("  movsx %s, %s\n", reg32_name(rhs_reg64), reg8_name(rhs_reg64));
    } else if (sz == 2) {
      write_file("  movsx %s, %s\n", reg32_name(lhs_reg64), reg16_name(lhs_reg64));
      write_file("  movsx %s, %s\n", reg32_name(rhs_reg64), reg16_name(rhs_reg64));
    } else if (sz == 4) {
      write_file("  movsxd %s, %s\n", lhs_reg64, reg32_name(lhs_reg64));
      write_file("  movsxd %s, %s\n", rhs_reg64, reg32_name(rhs_reg64));
    }
  }
}

static int reg_eq(const char *a, const char *b) { return a && b && !strcmp(a, b); }

static void emit_binop(const MirAsmCtx *ctx, const MirInst *in, const char *op, int commutative) {
  const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
  VReg lhs_v = in->src1;
  VReg rhs_v = in->src2;
  const char *lhs_reg = vreg_assigned_reg64(ctx, lhs_v);
  const char *rhs_reg = vreg_assigned_reg64(ctx, rhs_v);
  const char *work = dst_reg ? dst_reg : "rax";

  if (dst_reg && reg_eq(dst_reg, rhs_reg) && !reg_eq(dst_reg, lhs_reg)) {
    if (commutative) {
      VReg t_v = lhs_v;
      lhs_v = rhs_v;
      rhs_v = t_v;
      const char *t_r = lhs_reg;
      lhs_reg = rhs_reg;
      rhs_reg = t_r;
    } else {
      work = "rax";
    }
  }

  load_vreg_to_reg(ctx, lhs_v, work);
  if (rhs_reg) {
    write_file("  %s %s, %s\n", op, work, rhs_reg);
  } else {
    int off = vreg_stack_offset_or_neg1(ctx, rhs_v);
    if (off < 0)
      error("invalid rhs operand location [in emit_binop]");
    write_file("  %s %s, QWORD PTR [rbp - %d]\n", op, work, off);
  }

  if (!dst_reg || strcmp(work, dst_reg))
    store_reg_to_vreg(ctx, in->dst, work);
}

static void emit_mir_inst(MirAsmCtx *ctx, const MirInst *in) {
  if (!ctx || !ctx->mf || !ctx->mf->fn || !in)
    error("invalid MIR emit context");

  switch (in->op) {
  case MIR_OP_NOP:
    write_file("  nop\n");
    return;
  case MIR_OP_IMM: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    write_file("  mov %s, %ld\n", work, in->imm);
    emit_cast_reg(in->type, work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_MOV: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *src_reg = vreg_assigned_reg64(ctx, in->src1);
    if (dst_reg && src_reg) {
      if (strcmp(dst_reg, src_reg))
        write_file("  mov %s, %s\n", dst_reg, src_reg);
      return;
    }
    if (dst_reg) {
      load_vreg_to_reg(ctx, in->src1, dst_reg);
      return;
    }
    if (src_reg) {
      int dst_off = vreg_stack_offset_or_neg1(ctx, in->dst);
      if (dst_off < 0)
        error("invalid dst location [in MIR_OP_MOV]");
      write_file("  mov QWORD PTR [rbp - %d], %s\n", dst_off, src_reg);
      return;
    }
    int src_off = vreg_stack_offset_or_neg1(ctx, in->src1);
    int dst_off = vreg_stack_offset_or_neg1(ctx, in->dst);
    if (src_off < 0 || dst_off < 0)
      error("invalid stack location [in MIR_OP_MOV]");
    write_file("  mov r11, QWORD PTR [rbp - %d]\n", src_off);
    write_file("  mov QWORD PTR [rbp - %d], r11\n", dst_off);
    return;
  }
  case MIR_OP_LOAD_LOCAL: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    load_reg_from_local(in->offset, in->type, work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_STORE_LOCAL: {
    const char *src_reg = vreg_assigned_reg64(ctx, in->src1);
    if (src_reg) {
      store_reg_to_local_by64(in->offset, in->type, src_reg);
    } else {
      load_vreg_to_reg(ctx, in->src1, "r11");
      store_reg_to_local_by64(in->offset, in->type, "r11");
    }
    return;
  }
  case MIR_OP_ADDR_LOCAL: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    if (dst_reg) {
      write_file("  lea %s, [rbp - %d]\n", dst_reg, in->offset);
    } else {
      write_file("  lea rax, [rbp - %d]\n", in->offset);
      store_reg_to_vreg(ctx, in->dst, "rax");
    }
    return;
  }
  case MIR_OP_ADDR_SYMBOL:
    if (!in->var)
      error("missing symbol in MIR_OP_ADDR_SYMBOL");
    {
      const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
      const char *work = dst_reg ? dst_reg : "rax";
      if (in->var->is_static && in->var->block > 0) {
        write_file("  lea %s, %s%.*s.%d[rip]\n", work, ASM_PREFIX, in->var->len, in->var->name, in->var->block);
      } else if (in->var->is_static) {
        write_file("  lea %s, " ASM_SYM_FMT "[rip]\n", work, ASM_SYM_ARGS(in->var->len, in->var->name));
      } else {
        write_file("  mov %s, QWORD PTR [rip + " ASM_SYM_FMT "@GOTPCREL]\n", work,
                   ASM_SYM_ARGS(in->var->len, in->var->name));
      }
      if (!dst_reg)
        store_reg_to_vreg(ctx, in->dst, work);
      return;
    }
  case MIR_OP_ADDR_FUNC:
    if (!in->call_fn)
      error("missing function in MIR_OP_ADDR_FUNC");
    {
      const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
      const char *work = dst_reg ? dst_reg : "rax";
      write_file("  lea %s, " ASM_SYM_FMT "[rip]\n", work, ASM_SYM_ARGS(in->call_fn->len, in->call_fn->name));
      if (!dst_reg)
        store_reg_to_vreg(ctx, in->dst, work);
      return;
    }
  case MIR_OP_ADDR_STRING: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    write_file("  lea %s, [rip + .L.str%d]\n", work, in->offset);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_ADDR_ARRAY: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    write_file("  lea %s, [rip + .L.arr%d]\n", work, in->offset);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_ADDR_STRUCT_LITERAL: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    write_file("  lea %s, [rip + .L.struct%d]\n", work, in->offset);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_LOAD: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    load_vreg_to_reg(ctx, in->src1, work);
    load_reg_from_ptr(in->type, work, work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_STORE: {
    const char *ptr_reg = vreg_assigned_reg64(ctx, in->src1);
    const char *val_reg = vreg_assigned_reg64(ctx, in->src2);
    const char *ptr_work = ptr_reg ? ptr_reg : "rax";
    const char *val_work = val_reg ? val_reg : "r11";
    load_vreg_to_reg(ctx, in->src1, ptr_work);
    if (!val_reg)
      load_vreg_to_reg(ctx, in->src2, val_work);
    store_reg_to_ptr(in->type, ptr_work, val_work);
    return;
  }
  case MIR_OP_MEMCPY: {
    long size = in->imm;
    if (size < 0)
      error("invalid memcpy size [in MIR_OP_MEMCPY]");
    if (size == 0)
      return;

    // Use call-clobbered pointer regs for block copy; allocated vregs stay in callee-saved regs.
    load_vreg_to_reg(ctx, in->src1, "rdi");
    load_vreg_to_reg(ctx, in->src2, "rsi");

    if (size > 32) {
      write_file("  mov rcx, %ld\n", size);
      write_file("  rep movsb\n");
      return;
    }

    long off = 0;
    while (off + 8 <= size) {
      if (off == 0) {
        write_file("  mov r11, QWORD PTR [rsi]\n");
        write_file("  mov QWORD PTR [rdi], r11\n");
      } else {
        write_file("  mov r11, QWORD PTR [rsi + %ld]\n", off);
        write_file("  mov QWORD PTR [rdi + %ld], r11\n", off);
      }
      off += 8;
    }
    if (off + 4 <= size) {
      if (off == 0) {
        write_file("  mov r11d, DWORD PTR [rsi]\n");
        write_file("  mov DWORD PTR [rdi], r11d\n");
      } else {
        write_file("  mov r11d, DWORD PTR [rsi + %ld]\n", off);
        write_file("  mov DWORD PTR [rdi + %ld], r11d\n", off);
      }
      off += 4;
    }
    if (off + 2 <= size) {
      if (off == 0) {
        write_file("  mov r11w, WORD PTR [rsi]\n");
        write_file("  mov WORD PTR [rdi], r11w\n");
      } else {
        write_file("  mov r11w, WORD PTR [rsi + %ld]\n", off);
        write_file("  mov WORD PTR [rdi + %ld], r11w\n", off);
      }
      off += 2;
    }
    if (off < size) {
      if (off == 0) {
        write_file("  mov r11b, BYTE PTR [rsi]\n");
        write_file("  mov BYTE PTR [rdi], r11b\n");
      } else {
        write_file("  mov r11b, BYTE PTR [rsi + %ld]\n", off);
        write_file("  mov BYTE PTR [rdi + %ld], r11b\n", off);
      }
    }
    return;
  }
  case MIR_OP_CAST: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    load_vreg_to_reg(ctx, in->src1, work);
    emit_cast_reg(in->type, work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_ADD:
    emit_binop(ctx, in, "add", 1);
    return;
  case MIR_OP_SUB:
    emit_binop(ctx, in, "sub", 0);
    return;
  case MIR_OP_MUL:
    emit_binop(ctx, in, "imul", 1);
    return;
  case MIR_OP_SDIV:
  case MIR_OP_SMOD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    if (vreg_assigned_reg64(ctx, in->src2))
      load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  cqo\n");
    if (vreg_assigned_reg64(ctx, in->src2)) {
      write_file("  idiv rdi\n");
    } else {
      int off = vreg_stack_offset_or_neg1(ctx, in->src2);
      if (off < 0)
        error("invalid div rhs location [in MIR_OP_SDIV/SMOD]");
      write_file("  idiv QWORD PTR [rbp - %d]\n", off);
    }
    if (in->op == MIR_OP_SMOD)
      write_file("  mov rax, rdx\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_UDIV:
  case MIR_OP_UMOD:
    load_vreg_to_reg(ctx, in->src1, "rax");
    if (vreg_assigned_reg64(ctx, in->src2))
      load_vreg_to_reg(ctx, in->src2, "rdi");
    write_file("  xor rdx, rdx\n");
    if (vreg_assigned_reg64(ctx, in->src2)) {
      write_file("  div rdi\n");
    } else {
      int off = vreg_stack_offset_or_neg1(ctx, in->src2);
      if (off < 0)
        error("invalid div rhs location [in MIR_OP_UDIV/UMOD]");
      write_file("  div QWORD PTR [rbp - %d]\n", off);
    }
    if (in->op == MIR_OP_UMOD)
      write_file("  mov rax, rdx\n");
    store_reg_to_vreg(ctx, in->dst, "rax");
    return;
  case MIR_OP_EQ:
  case MIR_OP_NE:
  case MIR_OP_LT:
  case MIR_OP_LE: {
    const char *lhs_reg = vreg_assigned_reg64(ctx, in->src1);
    const char *rhs_reg = vreg_assigned_reg64(ctx, in->src2);
    const char *lhs_work = lhs_reg ? lhs_reg : "rax";
    const char *rhs_work = rhs_reg ? rhs_reg : "rdi";
    if (reg_eq(lhs_work, rhs_work))
      rhs_work = "r11";

    load_vreg_to_reg(ctx, in->src1, lhs_work);
    load_vreg_to_reg(ctx, in->src2, rhs_work);
    normalize_cmp_operands(in->type, lhs_work, rhs_work);
    write_file("  cmp %s, %s\n", lhs_work, rhs_work);

    const char *res_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *res_work = res_reg ? res_reg : "rax";
    if (in->op == MIR_OP_EQ)
      write_file("  sete %s\n", reg8_name(res_work));
    else if (in->op == MIR_OP_NE)
      write_file("  setne %s\n", reg8_name(res_work));
    else if (in->op == MIR_OP_LT) {
      if (in->type && in->type->is_unsigned)
        write_file("  setb %s\n", reg8_name(res_work));
      else
        write_file("  setl %s\n", reg8_name(res_work));
    } else {
      if (in->type && in->type->is_unsigned)
        write_file("  setbe %s\n", reg8_name(res_work));
      else
        write_file("  setle %s\n", reg8_name(res_work));
    }
    write_file("  movzx %s, %s\n", reg32_name(res_work), reg8_name(res_work));
    if (!res_reg)
      store_reg_to_vreg(ctx, in->dst, res_work);
    return;
  }
  case MIR_OP_BITAND:
    emit_binop(ctx, in, "and", 1);
    return;
  case MIR_OP_BITOR:
    emit_binop(ctx, in, "or", 1);
    return;
  case MIR_OP_BITXOR:
    emit_binop(ctx, in, "xor", 1);
    return;
  case MIR_OP_SHL:
  case MIR_OP_SHR: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *src2_reg = vreg_assigned_reg64(ctx, in->src2);
    const char *work = dst_reg && !reg_eq(dst_reg, src2_reg) ? dst_reg : "rax";
    load_vreg_to_reg(ctx, in->src1, work);
    if (src2_reg) {
      if (strcmp(src2_reg, "rcx"))
        write_file("  mov rcx, %s\n", src2_reg);
    } else {
      load_vreg_to_reg(ctx, in->src2, "rcx");
    }
    if (in->type && type_size(in->type) == 4) {
      if (in->op == MIR_OP_SHL) {
        write_file("  shl %s, cl\n", reg32_name(work));
      } else if (in->type->is_unsigned) {
        write_file("  shr %s, cl\n", reg32_name(work));
      } else {
        write_file("  sar %s, cl\n", reg32_name(work));
      }
    } else {
      if (in->op == MIR_OP_SHL) {
        write_file("  shl %s, cl\n", work);
      } else if (in->type && in->type->is_unsigned) {
        write_file("  shr %s, cl\n", work);
      } else {
        write_file("  sar %s, cl\n", work);
      }
    }
    if (!dst_reg || strcmp(work, dst_reg))
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_BITNOT: {
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    const char *work = dst_reg ? dst_reg : "rax";
    load_vreg_to_reg(ctx, in->src1, work);
    write_file("  not %s\n", work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_LABEL:
    emit_mir_label(ctx->mf, in->label);
    return;
  case MIR_OP_JMP:
    emit_mir_jmp(ctx->mf, in->label);
    return;
  case MIR_OP_JZ: {
    const char *src_reg = vreg_assigned_reg64(ctx, in->src1);
    if (src_reg) {
      write_file("  cmp %s, 0\n", src_reg);
    } else {
      int off = vreg_stack_offset_or_neg1(ctx, in->src1);
      if (off < 0)
        error("invalid JZ operand location [in MIR_OP_JZ]");
      write_file("  cmp QWORD PTR [rbp - %d], 0\n", off);
    }
    write_file("  je .Lmir.%.*s.%d\n", ctx->mf->fn->len, ctx->mf->fn->name, in->label);
    return;
  }
  case MIR_OP_CALL: {
    int reg_argc = in->argc < MIR_CALL_ARG_REGS ? in->argc : MIR_CALL_ARG_REGS;
    int stack_argc = in->argc - reg_argc;
    int align_pad = stack_argc & 1;

    // Prologue keeps rsp 16-byte aligned. For odd stack-arg count, reserve 8-byte pad first.
    if (align_pad)
      write_file("  sub rsp, 8\n");

    // Push only stack-passed args (arg7+), right-to-left.
    for (int a = in->argc - 1; a >= reg_argc; a--) {
      const char *arg_reg = vreg_assigned_reg64(ctx, in->args[a]);
      if (arg_reg) {
        write_file("  push %s\n", arg_reg);
      } else {
        int off = vreg_stack_offset_or_neg1(ctx, in->args[a]);
        if (off < 0)
          error("invalid call arg location [in MIR_OP_CALL]");
        write_file("  push QWORD PTR [rbp - %d]\n", off);
      }
    }

    // Move register-passed args directly into ABI arg regs.
    for (int a = 0; a < reg_argc; a++) {
      const char *dst = mir_arg_regs8[a];
      const char *arg_reg = vreg_assigned_reg64(ctx, in->args[a]);
      if (arg_reg) {
        if (strcmp(arg_reg, dst))
          write_file("  mov %s, %s\n", dst, arg_reg);
      } else {
        int off = vreg_stack_offset_or_neg1(ctx, in->args[a]);
        if (off < 0)
          error("invalid call arg location [in MIR_OP_CALL]");
        write_file("  mov %s, QWORD PTR [rbp - %d]\n", dst, off);
      }
    }

    const char *fnptr_reg = NULL;
    if (!in->call_fn) {
      fnptr_reg = vreg_assigned_reg64(ctx, in->src1);
      if (!fnptr_reg)
        load_vreg_to_reg(ctx, in->src1, "r10");
    }

    write_file("  mov rax, 0\n");
    if (in->call_fn) {
      write_file("  call " ASM_SYM_FMT "\n", ASM_SYM_ARGS(in->call_fn->len, in->call_fn->name));
    } else if (fnptr_reg) {
      write_file("  call %s\n", fnptr_reg);
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
