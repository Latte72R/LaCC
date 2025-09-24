
#include "lacc.h"

extern Node **code;
extern LVar *globals;
extern LVar *statics;
extern String *strings;
extern Array *arrays;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

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
    if (var->is_static) {
      write_file("  .local %.*s\n", var->len, var->name);
    } else {
      write_file("  .globl %.*s\n", var->len, var->name);
    }
    write_file("  .p2align 3\n");
    write_file("%.*s:\n", var->len, var->name);
    if (var->init_array) {
      write_array_data(var->init_array);
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
    write_file("  .local %.*s.%d\n", var->len, var->name, var->block);
    write_file("  .p2align 3\n");
    write_file("%.*s.%d:\n", var->len, var->name, var->block);
    if (var->init_array) {
      write_array_data(var->init_array);
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

void gen_rodata_section() {
  write_file("  .section .rodata\n");
  gen_string_literal();
}

void gen_data_section() {
  write_file("  .data\n");
  gen_global_variables();
  gen_static_variables();
  gen_array_literals();
}
