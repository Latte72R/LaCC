
#include "diagnostics.h"
#include "parser.h"
#include "platform.h"
#include "runtime.h"

#include "codegen_internal.h"

static void write_array_data(Array *arr) {
  for (int i = 0; i < arr->len; i++) {
    if (arr->byte == 1) {
      if (i >= arr->init) {
        write_file("  .byte 0\n");
      } else {
        write_file("  .byte %d\n", arr->val[i]);
      }
    } else if (arr->byte == 4) {
      if (i >= arr->init) {
        write_file("  .long 0\n");
      } else {
        write_file("  .long %d\n", arr->val[i]);
      }
    } else if (arr->byte == 8) {
      if (i >= arr->init) {
        write_file("  .quad 0\n");
      } else if (arr->str && arr->str[i]) {
        write_file("  .quad .L.str%d\n", arr->str[i]->id);
      } else {
        write_file("  .quad %d\n", arr->val[i]);
      }
    } else {
      error("invalid array type [in write_array_data]");
    }
  }
}

static void write_struct_data(StructLiteral *lit) {
  for (int i = 0; i < lit->size; i++)
    write_file("  .byte %d\n", lit->data[i]);
}

// 文字列リテラルの生成
void gen_string_literal() {
  for (String *str = strings; str; str = str->next) {
    write_file(".L.str%d:\n", str->id);
    write_file("  .asciz \"%.*s\"\n", str->len, str->text);
  }
}

// グローバル変数の生成
void gen_global_variables() {
  for (LVar *var = globals; var; var = var->next) {
    if (var->is_extern) {
      continue;
    }
#if !LACC_PLATFORM_APPLE
    if (var->is_static) {
      write_file("  .local " ASM_SYM_FMT "\n", ASM_SYM_ARGS(var->len, var->name));
    } else {
      write_file("  .globl " ASM_SYM_FMT "\n", ASM_SYM_ARGS(var->len, var->name));
    }
#else
    if (!var->is_static) {
      write_file("  .globl " ASM_SYM_FMT "\n", ASM_SYM_ARGS(var->len, var->name));
    }
#endif
    write_file("  .p2align 3\n");
    write_file(ASM_SYM_FMT ":\n", ASM_SYM_ARGS(var->len, var->name));
    if (var->init_array) {
      write_array_data(var->init_array);
    } else if (var->init_struct) {
      write_struct_data(var->init_struct);
    } else if (var->offset) {
      int sz = get_sizeof(var->type);
      if (sz == 1)
        write_file("  .byte %d\n", var->offset);
      else if (sz == 2)
        write_file("  .word %d\n", var->offset);
      else if (sz == 4)
        write_file("  .long %d\n", var->offset);
      else if (sz == 8)
        write_file("  .quad %d\n", var->offset);
      else
        error("invalid type size [in gen_global_variables]");
    } else {
      write_file("  .zero %d\n", get_sizeof(var->type));
    }
  }
}

// staticな変数の生成
void gen_static_variables() {
  for (LVar *var = statics; var; var = var->next) {
#if !LACC_PLATFORM_APPLE
    write_file("  .local %s%.*s.%d\n", ASM_PREFIX, var->len, var->name, var->block);
#endif
    write_file("  .p2align 3\n");
    write_file("%s%.*s.%d:\n", ASM_PREFIX, var->len, var->name, var->block);
    if (var->init_array) {
      write_array_data(var->init_array);
    } else if (var->init_struct) {
      write_struct_data(var->init_struct);
    } else if (var->offset) {
      int sz = get_sizeof(var->type);
      if (sz == 1)
        write_file("  .byte %d\n", var->offset);
      else if (sz == 2)
        write_file("  .word %d\n", var->offset);
      else if (sz == 4)
        write_file("  .long %d\n", var->offset);
      else if (sz == 8)
        write_file("  .quad %d\n", var->offset);
      else
        error("invalid type size [in gen_static_variables]");
    } else {
      write_file("  .zero %d\n", get_sizeof(var->type));
    }
  }
}

// 配列リテラルの生成
void gen_array_literals() {
  for (Array *arr = arrays; arr; arr = arr->next) {
    write_file(".L.arr%d:\n", arr->id);
    write_array_data(arr);
  }
}

static void gen_struct_literals() {
  for (StructLiteral *lit = struct_literals; lit; lit = lit->next) {
    if (!lit->needs_label)
      continue;
    write_file(".L.struct%d:\n", lit->id);
    write_struct_data(lit);
  }
}

void gen_rodata_section() {
#if LACC_PLATFORM_APPLE
  write_file("  .section __TEXT,__cstring,cstring_literals\n");
#else
  write_file("  .section .rodata\n");
#endif
  gen_string_literal();
}

void gen_data_section() {
#if LACC_PLATFORM_APPLE
  write_file("  .section __DATA,__data\n");
#else
  write_file("  .data\n");
#endif
  gen_global_variables();
  gen_static_variables();
  gen_array_literals();
  gen_struct_literals();
}
