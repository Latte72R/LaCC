#include "diagnostics.h"
#include "mir_internal.h"
#include "regalloc.h"
#include "platform.h"

#include <stdlib.h>
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
  const unsigned char *single_def_meta_known;
  const MirOp *single_def_meta_op;
  Type *const *single_def_meta_type;
  const int *single_def_meta_offset;
  const unsigned char *single_def_imm_known;
  const long *single_def_imm_value;
  const unsigned char *skip_emit_addr_local;
  const unsigned char *skip_emit_imm;
  const char *epilogue_label;
  int pending_jmp_label;
  int has_pending_jmp;
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

static int count_saved_mask_bits(unsigned mask) {
  int c = 0;
  for (int p = 0; p < RA_PREG_COUNT; p++) {
    if (!(mask & (1u << p)))
      continue;
    if (ra_preg_is_callee_saved(p))
      c++;
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
      // 32bitのmovは符号なしでゼロ拡張される
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

static void emit_cast_reg_from_reg(Type *type, const char *dst64, const char *src64) {
  if (!type || !dst64 || !src64)
    error("invalid cast register operands [in emit_cast_reg_from_reg]");
  switch (type->ty) {
  case TY_BOOL:
    write_file("  cmp %s, 0\n", src64);
    write_file("  setne %s\n", reg8_name(dst64));
    write_file("  movzx %s, %s\n", reg32_name(dst64), reg8_name(dst64));
    break;
  case TY_CHAR:
    if (type->is_unsigned)
      write_file("  movzx %s, %s\n", reg32_name(dst64), reg8_name(src64));
    else
      write_file("  movsx %s, %s\n", reg32_name(dst64), reg8_name(src64));
    break;
  case TY_SHORT:
    if (type->is_unsigned)
      write_file("  movzx %s, %s\n", reg32_name(dst64), reg16_name(src64));
    else
      write_file("  movsx %s, %s\n", reg32_name(dst64), reg16_name(src64));
    break;
  case TY_INT:
    if (type->is_unsigned)
      write_file("  mov %s, %s\n", reg32_name(dst64), reg32_name(src64));
    else
      write_file("  movsxd %s, %s\n", dst64, reg32_name(src64));
    break;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
    if (strcmp(dst64, src64))
      write_file("  mov %s, %s\n", dst64, src64);
    break;
  default:
    error("unsupported type in MIR cast [ty=%d]", type->ty);
  }
}

static unsigned long zext_u8(long imm) {
  unsigned char u8 = (unsigned char)imm;
  return (unsigned long)u8;
}

static unsigned long zext_u16(long imm) {
  unsigned short u16 = (unsigned short)imm;
  return (unsigned long)u16;
}

static unsigned long zext_u32(long imm) {
  unsigned int u32 = (unsigned int)imm;
  return (unsigned long)u32;
}

static long sext_i8(long imm) {
  unsigned char u8 = (unsigned char)imm;
  signed char s8 = (signed char)u8;
  return (long)s8;
}

static long sext_i16(long imm) {
  unsigned short u16 = (unsigned short)imm;
  short s16 = (short)u16;
  return (long)s16;
}

static long sext_i32(long imm) {
  unsigned int u32 = (unsigned int)imm;
  int s32 = (int)u32;
  return (long)s32;
}

static void emit_typed_imm_to_reg(Type *type, const char *reg64, long imm) {
  if (!reg64)
    error("missing destination register [in emit_typed_imm_to_reg]");

  if (!type) {
    error("missing type [in emit_typed_imm_to_reg]");
  }

  switch (type->ty) {
  case TY_BOOL:
    write_file("  mov %s, %lu\n", reg32_name(reg64), imm ? 1UL : 0UL);
    break;
  case TY_CHAR: {
    unsigned long u = zext_u8(imm);
    if (type->is_unsigned) {
      write_file("  mov %s, %lu\n", reg32_name(reg64), u);
    } else {
      write_file("  mov %s, %ld\n", reg8_name(reg64), (long)u);
      write_file("  movsx %s, %s\n", reg64, reg8_name(reg64));
    }
    break;
  }
  case TY_SHORT: {
    unsigned long u = zext_u16(imm);
    if (type->is_unsigned) {
      write_file("  mov %s, %lu\n", reg32_name(reg64), u);
    } else {
      write_file("  mov %s, %ld\n", reg16_name(reg64), (long)u);
      write_file("  movsx %s, %s\n", reg64, reg16_name(reg64));
    }
    break;
  }
  case TY_INT: {
    unsigned long u = zext_u32(imm);
    if (type->is_unsigned) {
      write_file("  mov %s, %lu\n", reg32_name(reg64), u);
    } else {
      long s = sext_i32(imm);
      write_file("  mov %s, %ld\n", reg64, s);
    }
    break;
  }
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
    if (type->is_unsigned) {
      write_file("  mov %s, %lu\n", reg64, (unsigned long)imm);
    } else {
      write_file("  mov %s, %ld\n", reg64, imm);
    }
    break;
  default:
    error("unsupported type in MIR imm [ty=%d]", type->ty);
  }
}

static void emit_typed_imm_to_stack_direct(Type *type, int off, long imm) {
  // Spill slots are always 8-byte cells; materialize a canonical 64-bit value
  // in a temp register first, then store full width.
  emit_typed_imm_to_reg(type, "r11", imm);
  write_file("  mov QWORD PTR [rbp - %d], r11\n", off);
}

static long typed_imm_canonical_value(Type *type, long imm) {
  if (!type)
    return imm;
  switch (type->ty) {
  case TY_BOOL:
    return imm ? 1 : 0;
  case TY_CHAR:
    if (type->is_unsigned)
      return (long)zext_u8(imm);
    return sext_i8(imm);
  case TY_SHORT:
    if (type->is_unsigned)
      return (long)zext_u16(imm);
    return sext_i16(imm);
  case TY_INT:
    if (type->is_unsigned)
      return (long)zext_u32(imm);
    return sext_i32(imm);
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
    if (type->is_unsigned)
      return (long)(unsigned long)imm;
    return imm;
  default:
    return imm;
  }
}

static int is_signext_i32(long v) { return v >= (-2147483647L - 1L) && v <= 2147483647L; }

static int can_emit_typed_imm_to_local_direct(Type *type, long imm) {
  if (!type)
    return 0;
  if (type->ty == TY_BOOL || type->ty == TY_CHAR || type->ty == TY_SHORT || type->ty == TY_INT)
    return 1;
  if (type->ty == TY_LONG || type->ty == TY_LONGLONG || type->ty == TY_PTR || type->ty == TY_ARGARR ||
      type->ty == TY_ARR)
    return is_signext_i32(typed_imm_canonical_value(type, imm));
  return 0;
}

static int emit_typed_imm_to_local_direct(Type *type, int off, long imm) {
  long v = typed_imm_canonical_value(type, imm);
  if (!can_emit_typed_imm_to_local_direct(type, imm))
    return 0;
  switch (type->ty) {
  case TY_BOOL:
    write_file("  mov BYTE PTR [rbp - %d], %ld\n", off, v ? 1L : 0L);
    return 1;
  case TY_CHAR:
    write_file("  mov BYTE PTR [rbp - %d], %ld\n", off, v);
    return 1;
  case TY_SHORT:
    write_file("  mov WORD PTR [rbp - %d], %ld\n", off, v);
    return 1;
  case TY_INT:
    write_file("  mov DWORD PTR [rbp - %d], %ld\n", off, v);
    return 1;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
    write_file("  mov QWORD PTR [rbp - %d], %ld\n", off, v);
    return 1;
  default:
    return 0;
  }
}

static void build_single_def_imm_info(const MirFunction *mf, unsigned char *known, long *value) {
  if (!mf || mf->next_vreg <= 0 || !known || !value)
    return;

  int *def_count = calloc(mf->next_vreg, sizeof(int));
  if (!def_count)
    error("memory allocation failed [in build_single_def_imm_info]");

  for (int v = 0; v < mf->next_vreg; v++) {
    known[v] = 0;
    value[v] = 0;
  }

  for (int i = 0; i < mf->inst_len; i++) {
    const MirInst *in = &mf->insts[i];
    if (in->dst < 0 || in->dst >= mf->next_vreg)
      continue;
    int v = in->dst;
    def_count[v]++;
    if (def_count[v] != 1) {
      known[v] = 0;
      continue;
    }
    if (in->op == MIR_OP_IMM) {
      known[v] = 1;
      value[v] = typed_imm_canonical_value(in->type, in->imm);
    } else {
      known[v] = 0;
    }
  }

  for (int v = 0; v < mf->next_vreg; v++) {
    if (def_count[v] != 1)
      known[v] = 0;
  }

  free(def_count);
}

static int vreg_single_def_imm(const MirAsmCtx *ctx, VReg vreg, long *imm) {
  if (!ctx || !ctx->single_def_imm_known || !ctx->single_def_imm_value)
    return 0;
  if (!ctx->mf || vreg < 0 || vreg >= ctx->mf->next_vreg)
    return 0;
  if (!ctx->single_def_imm_known[vreg])
    return 0;
  if (imm)
    *imm = ctx->single_def_imm_value[vreg];
  return 1;
}

static void build_single_def_meta_info(const MirFunction *mf, unsigned char *known, MirOp *op, Type **type,
                                       int *offset) {
  if (!mf || mf->next_vreg <= 0 || !known || !op || !type || !offset)
    return;

  int *def_count = calloc(mf->next_vreg, sizeof(int));
  if (!def_count)
    error("memory allocation failed [in build_single_def_meta_info]");

  for (int v = 0; v < mf->next_vreg; v++) {
    known[v] = 0;
    op[v] = MIR_OP_NOP;
    type[v] = NULL;
    offset[v] = 0;
  }

  for (int i = 0; i < mf->inst_len; i++) {
    const MirInst *in = &mf->insts[i];
    if (in->dst < 0 || in->dst >= mf->next_vreg)
      continue;
    int v = in->dst;
    def_count[v]++;
    if (def_count[v] != 1) {
      known[v] = 0;
      continue;
    }
    known[v] = 1;
    op[v] = in->op;
    type[v] = in->type;
    offset[v] = in->offset;
  }

  for (int v = 0; v < mf->next_vreg; v++) {
    if (def_count[v] != 1)
      known[v] = 0;
  }

  free(def_count);
}

static int vreg_single_def_meta(const MirAsmCtx *ctx, VReg vreg, MirOp *op, Type **type) {
  if (!ctx || !ctx->single_def_meta_known || !ctx->single_def_meta_op || !ctx->single_def_meta_type)
    return 0;
  if (!ctx->mf || vreg < 0 || vreg >= ctx->mf->next_vreg)
    return 0;
  if (!ctx->single_def_meta_known[vreg])
    return 0;
  if (op)
    *op = ctx->single_def_meta_op[vreg];
  if (type)
    *type = ctx->single_def_meta_type[vreg];
  return 1;
}

static int vreg_single_def_addr_local_offset(const MirAsmCtx *ctx, VReg vreg, int *offset) {
  MirOp op = MIR_OP_NOP;
  if (!vreg_single_def_meta(ctx, vreg, &op, NULL))
    return 0;
  if (op != MIR_OP_ADDR_LOCAL || !ctx->single_def_meta_offset)
    return 0;
  if (!ctx->mf || vreg < 0 || vreg >= ctx->mf->next_vreg)
    return 0;
  if (offset)
    *offset = ctx->single_def_meta_offset[vreg];
  return 1;
}

static int same_scalar_type(Type *a, Type *b) {
  if (!a || !b)
    return 0;
  return a->ty == b->ty && a->is_unsigned == b->is_unsigned;
}

static int cast_source_is_known_canonical(const MirAsmCtx *ctx, VReg src, Type *to_type) {
  MirOp op = MIR_OP_NOP;
  Type *from_type = NULL;
  if (!vreg_single_def_meta(ctx, src, &op, &from_type))
    return 0;
  if (!same_scalar_type(from_type, to_type))
    return 0;
  return op == MIR_OP_IMM || op == MIR_OP_LOAD || op == MIR_OP_LOAD_LOCAL || op == MIR_OP_CAST;
}

static int is_direct_local_store_pattern(const MirFunction *mf, const unsigned char *single_def_meta_known,
                                         const MirOp *single_def_meta_op, const MirInst *in) {
  if (!mf || !single_def_meta_known || !single_def_meta_op || !in)
    return 0;
  if (in->op != MIR_OP_STORE || !in->type || in->type->ty == TY_BOOL)
    return 0;
  if (in->src1 < 0 || in->src1 >= mf->next_vreg)
    return 0;
  if (!single_def_meta_known[in->src1])
    return 0;
  return single_def_meta_op[in->src1] == MIR_OP_ADDR_LOCAL;
}

static int can_consume_imm_use_without_materialize(const MirInst *in, int is_src1, long imm) {
  if (!in)
    return 0;
  if (in->op == MIR_OP_ADD || in->op == MIR_OP_SUB || in->op == MIR_OP_MUL)
    return 1;
  if (in->op == MIR_OP_STORE_LOCAL && is_src1)
    return can_emit_typed_imm_to_local_direct(in->type, imm);
  return 0;
}

static void consume_vreg_for_skip_emit(const MirFunction *mf, unsigned char *skip_emit_addr_local,
                                       unsigned char *skip_emit_imm, VReg vreg, int keep_addr_local,
                                       int keep_imm, const char *role) {
  if (!mf || !skip_emit_addr_local || !skip_emit_imm || !role)
    return;
  if (vreg < 0 || vreg >= mf->next_vreg)
    error("invalid %s vreg [in build_skip_emit_info]", role);
  if (skip_emit_addr_local[vreg] && !keep_addr_local)
    skip_emit_addr_local[vreg] = 0;
  if (skip_emit_imm[vreg] && !keep_imm)
    skip_emit_imm[vreg] = 0;
}

static void build_skip_emit_info(const MirFunction *mf, const unsigned char *single_def_meta_known,
                                 const MirOp *single_def_meta_op, const unsigned char *single_def_imm_known,
                                 const long *single_def_imm_value, unsigned char *skip_emit_addr_local,
                                 unsigned char *skip_emit_imm) {
  if (!mf || mf->next_vreg <= 0 || !single_def_meta_known || !single_def_meta_op || !skip_emit_addr_local ||
      !skip_emit_imm || !single_def_imm_known || !single_def_imm_value)
    return;

  for (int v = 0; v < mf->next_vreg; v++) {
    skip_emit_addr_local[v] = 0;
    skip_emit_imm[v] = 0;
    if (!single_def_meta_known[v])
      continue;
    if (single_def_meta_op[v] == MIR_OP_ADDR_LOCAL)
      skip_emit_addr_local[v] = 1;
    if (single_def_meta_op[v] == MIR_OP_IMM)
      skip_emit_imm[v] = 1;
  }

  for (int i = 0; i < mf->inst_len; i++) {
    const MirInst *in = &mf->insts[i];
    int allow_direct_local_store_use = is_direct_local_store_pattern(mf, single_def_meta_known, single_def_meta_op, in);

    if (in->src1 != MIR_INVALID_VREG) {
      int keep_imm = 0;
      if (in->src1 >= 0 && in->src1 < mf->next_vreg && single_def_imm_known[in->src1])
        keep_imm = can_consume_imm_use_without_materialize(in, 1, single_def_imm_value[in->src1]);
      consume_vreg_for_skip_emit(mf, skip_emit_addr_local, skip_emit_imm, in->src1,
                                 in->op == MIR_OP_LOAD || allow_direct_local_store_use, keep_imm, "src1");
    }

    if (in->src2 != MIR_INVALID_VREG) {
      int keep_imm = allow_direct_local_store_use;
      if (in->src2 >= 0 && in->src2 < mf->next_vreg && single_def_imm_known[in->src2])
        keep_imm = keep_imm || can_consume_imm_use_without_materialize(in, 0, single_def_imm_value[in->src2]);
      consume_vreg_for_skip_emit(mf, skip_emit_addr_local, skip_emit_imm, in->src2, 0, keep_imm, "src2");
    }

    if (in->op == MIR_OP_CALL) {
      for (int a = 0; a < in->argc; a++)
        consume_vreg_for_skip_emit(mf, skip_emit_addr_local, skip_emit_imm, in->args[a], 0, 0, "call arg");
    }
  }
}

static void emit_mir_label(const MirFunction *mf, int label) {
  write_file(".Lmir.%.*s.%d:\n", mf->fn->len, mf->fn->name, label);
}

static void emit_mir_jmp(const MirFunction *mf, int label) {
  write_file("  jmp .Lmir.%.*s.%d\n", mf->fn->len, mf->fn->name, label);
}

static void flush_pending_jmp(MirAsmCtx *ctx) {
  if (!ctx)
    return;
  if (!ctx->has_pending_jmp)
    return;
  emit_mir_jmp(ctx->mf, ctx->pending_jmp_label);
  ctx->has_pending_jmp = 0;
  ctx->pending_jmp_label = MIR_INVALID_LABEL;
}

static int can_ret_fallthrough_to_epilogue(const MirFunction *mf, int inst_idx) {
  if (!mf || inst_idx < 0 || inst_idx >= mf->inst_len)
    return 0;
  for (int i = inst_idx + 1; i < mf->inst_len; i++) {
    MirOp op = mf->insts[i].op;
    if (op == MIR_OP_LABEL || op == MIR_OP_NOP)
      continue;
    return 0;
  }
  return 1;
}

static int cmp_operand_size(Type *type) {
  if (!type)
    return 8;
  switch (type->ty) {
  case TY_BOOL:
  case TY_CHAR:
    return 1;
  case TY_SHORT:
    return 2;
  case TY_INT:
    return 4;
  case TY_LONG:
  case TY_LONGLONG:
  case TY_PTR:
  case TY_ARGARR:
  case TY_ARR:
  case TY_VOID:
    return 8;
  default:
    return 8;
  }
}

static void emit_cmp_reg_reg(Type *type, const char *lhs_reg64, const char *rhs_reg64) {
  int sz = cmp_operand_size(type);
  if (sz == 1) {
    write_file("  cmp %s, %s\n", reg8_name(lhs_reg64), reg8_name(rhs_reg64));
  } else if (sz == 2) {
    write_file("  cmp %s, %s\n", reg16_name(lhs_reg64), reg16_name(rhs_reg64));
  } else if (sz == 4) {
    write_file("  cmp %s, %s\n", reg32_name(lhs_reg64), reg32_name(rhs_reg64));
  } else {
    write_file("  cmp %s, %s\n", lhs_reg64, rhs_reg64);
  }
}

static int cmp_imm_encodable(int sz, long imm) {
  if (sz == 1)
    return sext_i8(imm) == imm || (long)zext_u8(imm) == imm;
  if (sz == 2)
    return sext_i16(imm) == imm || (long)zext_u16(imm) == imm;
  if (sz == 4)
    return sext_i32(imm) == imm || (long)zext_u32(imm) == imm;
  return is_signext_i32(imm);
}

static int emit_cmp_reg_imm(Type *type, const char *lhs_reg64, long imm) {
  int sz = cmp_operand_size(type);
  if (!cmp_imm_encodable(sz, imm))
    return 0;
  if (sz == 1) {
    write_file("  cmp %s, %ld\n", reg8_name(lhs_reg64), imm);
  } else if (sz == 2) {
    write_file("  cmp %s, %ld\n", reg16_name(lhs_reg64), imm);
  } else if (sz == 4) {
    write_file("  cmp %s, %ld\n", reg32_name(lhs_reg64), imm);
  } else {
    write_file("  cmp %s, %ld\n", lhs_reg64, imm);
  }
  return 1;
}

static void emit_cmp_reg_zero(Type *type, const char *reg64) {
  int sz = cmp_operand_size(type);
  if (sz == 1) {
    write_file("  cmp %s, 0\n", reg8_name(reg64));
  } else if (sz == 2) {
    write_file("  cmp %s, 0\n", reg16_name(reg64));
  } else if (sz == 4) {
    write_file("  cmp %s, 0\n", reg32_name(reg64));
  } else {
    write_file("  cmp %s, 0\n", reg64);
  }
}

static void emit_cmp_stack_zero(Type *type, int off) {
  int sz = cmp_operand_size(type);
  if (sz == 1) {
    write_file("  cmp BYTE PTR [rbp - %d], 0\n", off);
  } else if (sz == 2) {
    write_file("  cmp WORD PTR [rbp - %d], 0\n", off);
  } else if (sz == 4) {
    write_file("  cmp DWORD PTR [rbp - %d], 0\n", off);
  } else {
    write_file("  cmp QWORD PTR [rbp - %d], 0\n", off);
  }
}

static const char *jcc_mnemonic(long cc, Type *type) {
  switch (cc) {
  case MIR_CC_EQ:
    return "je";
  case MIR_CC_NE:
    return "jne";
  case MIR_CC_LT:
    return (type && type->is_unsigned) ? "jb" : "jl";
  case MIR_CC_LE:
    return (type && type->is_unsigned) ? "jbe" : "jle";
  case MIR_CC_GT:
    return (type && type->is_unsigned) ? "ja" : "jg";
  case MIR_CC_GE:
    return (type && type->is_unsigned) ? "jae" : "jge";
  default:
    error("invalid MIR JCC condition [cc=%ld]", cc);
  }
  return "je";
}

static int reg_eq(const char *a, const char *b) { return a && b && !strcmp(a, b); }

static void emit_binop(const MirAsmCtx *ctx, const MirInst *in, const char *op, int commutative) {
  const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
  VReg lhs_v = in->src1;
  VReg rhs_v = in->src2;
  long lhs_imm = 0;
  int lhs_is_imm = vreg_single_def_imm(ctx, lhs_v, &lhs_imm);
  long rhs_imm = 0;
  int rhs_is_imm = vreg_single_def_imm(ctx, rhs_v, &rhs_imm);
  if (!rhs_is_imm && commutative) {
    if (lhs_is_imm) {
      VReg t = lhs_v;
      lhs_v = rhs_v;
      rhs_v = t;
      rhs_is_imm = 1;
      rhs_imm = lhs_imm;
      lhs_is_imm = 0;
      lhs_imm = 0;
    }
  }
  const char *lhs_reg = vreg_assigned_reg64(ctx, lhs_v);
  const char *rhs_reg = vreg_assigned_reg64(ctx, rhs_v);
  const char *work = dst_reg ? dst_reg : "rax";

  if (!rhs_is_imm && dst_reg && reg_eq(dst_reg, rhs_reg) && !reg_eq(dst_reg, lhs_reg)) {
    if (commutative) {
      VReg t_v = lhs_v;
      lhs_v = rhs_v;
      rhs_v = t_v;
      long t_imm = rhs_imm;
      rhs_imm = 0;
      rhs_is_imm = vreg_single_def_imm(ctx, rhs_v, &rhs_imm);
      if (!rhs_is_imm)
        rhs_imm = t_imm;
      lhs_is_imm = vreg_single_def_imm(ctx, lhs_v, &lhs_imm);
      const char *t_r = lhs_reg;
      lhs_reg = rhs_reg;
      rhs_reg = t_r;
    } else {
      work = "rax";
    }
  }

  if (lhs_is_imm)
    emit_typed_imm_to_reg(in->type, work, lhs_imm);
  else
    load_vreg_to_reg(ctx, lhs_v, work);

  int can_use_imm = rhs_is_imm && (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0 || strcmp(op, "imul") == 0) &&
                    is_signext_i32(rhs_imm);
  if (can_use_imm) {
    write_file("  %s %s, %ld\n", op, work, rhs_imm);
  } else if (rhs_is_imm) {
    const char *rhs_work = reg_eq(work, "r11") ? "rdi" : "r11";
    emit_typed_imm_to_reg(in->type, rhs_work, rhs_imm);
    write_file("  %s %s, %s\n", op, work, rhs_work);
  } else if (rhs_reg) {
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

static void emit_mir_inst(MirAsmCtx *ctx, const MirInst *in, int inst_idx) {
  if (!ctx || !ctx->mf || !ctx->mf->fn || !in)
    error("invalid MIR emit context");

  if (in->op == MIR_OP_LABEL) {
    if (ctx->has_pending_jmp && ctx->pending_jmp_label == in->label) {
      ctx->has_pending_jmp = 0;
      ctx->pending_jmp_label = MIR_INVALID_LABEL;
    } else {
      flush_pending_jmp(ctx);
    }
    emit_mir_label(ctx->mf, in->label);
    return;
  }

  if (in->op == MIR_OP_JMP) {
    flush_pending_jmp(ctx);
    ctx->has_pending_jmp = 1;
    ctx->pending_jmp_label = in->label;
    return;
  }

  flush_pending_jmp(ctx);

  switch (in->op) {
  case MIR_OP_NOP:
    write_file("  nop\n");
    return;
  case MIR_OP_IMM: {
    if (ctx->skip_emit_imm && in->dst >= 0 && in->dst < ctx->mf->next_vreg && ctx->skip_emit_imm[in->dst])
      return;
    const char *dst_reg = vreg_assigned_reg64(ctx, in->dst);
    if (dst_reg) {
      emit_typed_imm_to_reg(in->type, dst_reg, in->imm);
      return;
    }
    int dst_off = vreg_stack_offset_or_neg1(ctx, in->dst);
    if (dst_off < 0)
      error("invalid dst location [in MIR_OP_IMM]");
    emit_typed_imm_to_stack_direct(in->type, dst_off, in->imm);
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
    long imm = 0;
    if (vreg_single_def_imm(ctx, in->src1, &imm) && emit_typed_imm_to_local_direct(in->type, in->offset, imm))
      return;
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
    if (ctx->skip_emit_addr_local && in->dst >= 0 && in->dst < ctx->mf->next_vreg &&
        ctx->skip_emit_addr_local[in->dst])
      return;
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
    int local_off = 0;
    if (vreg_single_def_addr_local_offset(ctx, in->src1, &local_off)) {
      load_reg_from_local(local_off, in->type, work);
      if (!dst_reg)
        store_reg_to_vreg(ctx, in->dst, work);
      return;
    }
    load_vreg_to_reg(ctx, in->src1, work);
    load_reg_from_ptr(in->type, work, work);
    if (!dst_reg)
      store_reg_to_vreg(ctx, in->dst, work);
    return;
  }
  case MIR_OP_STORE: {
    int local_off = 0;
    if (vreg_single_def_addr_local_offset(ctx, in->src1, &local_off) && !(in->type && in->type->ty == TY_BOOL)) {
      long imm = 0;
      if (vreg_single_def_imm(ctx, in->src2, &imm) && emit_typed_imm_to_local_direct(in->type, local_off, imm))
        return;
      const char *val_reg = vreg_assigned_reg64(ctx, in->src2);
      if (val_reg) {
        store_reg_to_local_by64(local_off, in->type, val_reg);
      } else {
        load_vreg_to_reg(ctx, in->src2, "r11");
        store_reg_to_local_by64(local_off, in->type, "r11");
      }
      return;
    }
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
    const char *src_reg = vreg_assigned_reg64(ctx, in->src1);
    const char *work = dst_reg ? dst_reg : "rax";
    if (src_reg && cast_source_is_known_canonical(ctx, in->src1, in->type)) {
      if (strcmp(work, src_reg))
        write_file("  mov %s, %s\n", work, src_reg);
    } else if (src_reg) {
      emit_cast_reg_from_reg(in->type, work, src_reg);
    } else {
      load_vreg_to_reg(ctx, in->src1, work);
      emit_cast_reg(in->type, work);
    }
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
    VReg lhs_v = in->src1;
    VReg rhs_v = in->src2;
    long rhs_imm = 0;
    int rhs_is_imm = vreg_single_def_imm(ctx, rhs_v, &rhs_imm);
    if (!rhs_is_imm && (in->op == MIR_OP_EQ || in->op == MIR_OP_NE)) {
      long lhs_imm = 0;
      if (vreg_single_def_imm(ctx, lhs_v, &lhs_imm)) {
        lhs_v = in->src2;
        rhs_v = in->src1;
        rhs_is_imm = 1;
        rhs_imm = lhs_imm;
      }
    }

    const char *lhs_reg = vreg_assigned_reg64(ctx, lhs_v);
    const char *rhs_reg = vreg_assigned_reg64(ctx, rhs_v);
    const char *lhs_work = lhs_reg ? lhs_reg : "rax";
    const char *rhs_work = rhs_reg ? rhs_reg : "rdi";
    if (reg_eq(lhs_work, rhs_work))
      rhs_work = "r11";

    load_vreg_to_reg(ctx, lhs_v, lhs_work);
    if (!(rhs_is_imm && emit_cmp_reg_imm(in->type, lhs_work, rhs_imm))) {
      load_vreg_to_reg(ctx, rhs_v, rhs_work);
      emit_cmp_reg_reg(in->type, lhs_work, rhs_work);
    }

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
    long shift_imm = 0;
    int shift_is_imm = vreg_single_def_imm(ctx, in->src2, &shift_imm) && shift_imm >= 0 && shift_imm <= 255;
    const char *work = (shift_is_imm || (dst_reg && !reg_eq(dst_reg, src2_reg))) && dst_reg ? dst_reg : "rax";
    load_vreg_to_reg(ctx, in->src1, work);
    if (shift_is_imm) {
      if (in->type && type_size(in->type) == 4) {
        if (in->op == MIR_OP_SHL) {
          write_file("  shl %s, %ld\n", reg32_name(work), shift_imm);
        } else if (in->type->is_unsigned) {
          write_file("  shr %s, %ld\n", reg32_name(work), shift_imm);
        } else {
          write_file("  sar %s, %ld\n", reg32_name(work), shift_imm);
        }
      } else {
        if (in->op == MIR_OP_SHL) {
          write_file("  shl %s, %ld\n", work, shift_imm);
        } else if (in->type && in->type->is_unsigned) {
          write_file("  shr %s, %ld\n", work, shift_imm);
        } else {
          write_file("  sar %s, %ld\n", work, shift_imm);
        }
      }
    } else if (src2_reg) {
      if (strcmp(src2_reg, "rcx"))
        write_file("  mov rcx, %s\n", src2_reg);
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
    } else {
      load_vreg_to_reg(ctx, in->src2, "rcx");
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
  case MIR_OP_JZ: {
    const char *src_reg = vreg_assigned_reg64(ctx, in->src1);
    if (src_reg) {
      emit_cmp_reg_zero(in->type, src_reg);
    } else {
      int off = vreg_stack_offset_or_neg1(ctx, in->src1);
      if (off < 0)
        error("invalid JZ operand location [in MIR_OP_JZ]");
      emit_cmp_stack_zero(in->type, off);
    }
    write_file("  je .Lmir.%.*s.%d\n", ctx->mf->fn->len, ctx->mf->fn->name, in->label);
    return;
  }
  case MIR_OP_JCC: {
    long rhs_imm = 0;
    int rhs_is_imm = vreg_single_def_imm(ctx, in->src2, &rhs_imm);
    const char *lhs_reg = vreg_assigned_reg64(ctx, in->src1);
    const char *rhs_reg = vreg_assigned_reg64(ctx, in->src2);
    const char *lhs_work = lhs_reg ? lhs_reg : "rax";
    const char *rhs_work = rhs_reg ? rhs_reg : "rdi";
    if (reg_eq(lhs_work, rhs_work))
      rhs_work = "r11";

    load_vreg_to_reg(ctx, in->src1, lhs_work);
    if (!(rhs_is_imm && emit_cmp_reg_imm(in->type, lhs_work, rhs_imm))) {
      load_vreg_to_reg(ctx, in->src2, rhs_work);
      emit_cmp_reg_reg(in->type, lhs_work, rhs_work);
    }
    write_file("  %s .Lmir.%.*s.%d\n", jcc_mnemonic(in->imm, in->type), ctx->mf->fn->len, ctx->mf->fn->name,
               in->label);
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
    if (in->type && in->type->ty == TY_BOOL) {
      write_file("  cmp rax, 0\n");
      write_file("  setne al\n");
      write_file("  movzx eax, al\n");
    }
    if (!can_ret_fallthrough_to_epilogue(ctx->mf, inst_idx))
      write_file("  jmp %s\n", ctx->epilogue_label);
    return;
  default:
    error("unsupported MIR op in asm emitter [op=%d]", in->op);
  }
}

void emit_mir_function(const MirFunction *mf) {
  if (!mf || !mf->fn)
    error("invalid MIR function in asm emitter");

  RegAllocResult ra = {0};
  regalloc_run(mf, &ra);
  unsigned char *single_def_meta_known = NULL;
  MirOp *single_def_meta_op = NULL;
  Type **single_def_meta_type = NULL;
  int *single_def_meta_offset = NULL;
  unsigned char *single_def_imm_known = NULL;
  long *single_def_imm_value = NULL;
  unsigned char *skip_emit_addr_local = NULL;
  unsigned char *skip_emit_imm = NULL;
  if (mf->next_vreg > 0) {
    single_def_meta_known = calloc(mf->next_vreg, sizeof(unsigned char));
    single_def_meta_op = calloc(mf->next_vreg, sizeof(MirOp));
    single_def_meta_type = calloc(mf->next_vreg, sizeof(Type *));
    single_def_meta_offset = calloc(mf->next_vreg, sizeof(int));
    single_def_imm_known = calloc(mf->next_vreg, sizeof(unsigned char));
    single_def_imm_value = calloc(mf->next_vreg, sizeof(long));
    skip_emit_addr_local = calloc(mf->next_vreg, sizeof(unsigned char));
    skip_emit_imm = calloc(mf->next_vreg, sizeof(unsigned char));
    if (!single_def_meta_known || !single_def_meta_op || !single_def_meta_type || !single_def_meta_offset ||
        !single_def_imm_known || !single_def_imm_value || !skip_emit_addr_local || !skip_emit_imm)
      error("memory allocation failed [in emit_mir_function single-def imm info]");
    build_single_def_meta_info(mf, single_def_meta_known, single_def_meta_op, single_def_meta_type,
                               single_def_meta_offset);
    build_single_def_imm_info(mf, single_def_imm_known, single_def_imm_value);
    build_skip_emit_info(mf, single_def_meta_known, single_def_meta_op, single_def_imm_known, single_def_imm_value,
                         skip_emit_addr_local, skip_emit_imm);
  }

  MirAsmCtx ctx = {0};
  ctx.mf = mf;
  ctx.ra = &ra;
  ctx.single_def_meta_known = single_def_meta_known;
  ctx.single_def_meta_op = single_def_meta_op;
  ctx.single_def_meta_type = single_def_meta_type;
  ctx.single_def_meta_offset = single_def_meta_offset;
  ctx.single_def_imm_known = single_def_imm_known;
  ctx.single_def_imm_value = single_def_imm_value;
  ctx.skip_emit_addr_local = skip_emit_addr_local;
  ctx.skip_emit_imm = skip_emit_imm;
  ctx.spill_base = align_up(mf->fn->offset, 8);
  int save_count = count_saved_mask_bits(ra.used_reg_mask);
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
    if (!ra_preg_is_callee_saved(p))
      continue;
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
  ctx.has_pending_jmp = 0;
  ctx.pending_jmp_label = MIR_INVALID_LABEL;

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
    emit_mir_inst(&ctx, &mf->insts[i], i);
  flush_pending_jmp(&ctx);

  write_file("%s:\n", ctx.epilogue_label);
  for (int p = RA_PREG_COUNT - 1; p >= 0; p--) {
    if (!ra_preg_is_callee_saved(p))
      continue;
    if (ctx.save_slot[p] >= 0)
      write_file("  mov %s, QWORD PTR [rbp - %d]\n", ra_preg64(p), ctx.save_slot[p]);
  }
  write_file("  mov rsp, rbp\n");
  write_file("  pop rbp\n");
  write_file("  ret\n");

  free(skip_emit_imm);
  free(skip_emit_addr_local);
  free(single_def_meta_offset);
  free(single_def_meta_type);
  free(single_def_meta_op);
  free(single_def_meta_known);
  free(single_def_imm_value);
  free(single_def_imm_known);
  regalloc_free(&ra);
}
