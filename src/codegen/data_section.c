
#include "lacc.h"

extern Node **code;
extern LVar *globals;
extern LVar *statics;
extern String *strings;
extern Array *arrays;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

// 文字列リテラルの生成
void gen_string_literal() {
  for (String *str = strings; str->next; str = str->next) {
    write_file(".L.str%d:\n", str->id);
    write_file("  .string \"%.*s\"\n", str->len, str->text);
  }
}

// グローバル変数の生成
void gen_global_variables() {
  for (LVar *var = globals; var->next; var = var->next) {
    if (var->ext) {
      continue;
    }
    if (var->is_static) {
      write_file("  .local %.*s\n", var->len, var->name);
    } else {
      write_file("  .globl %.*s\n", var->len, var->name);
    }
    write_file("  .p2align 3\n");
    write_file("%.*s:\n", var->len, var->name);
    if (var->offset) {
      write_file("  .long %d\n", var->offset);
    } else {
      write_file("  .zero %d\n", get_sizeof(var->type));
    }
  }
}

// staticな変数の生成
void gen_static_variables() {
  for (LVar *var = statics; var->next; var = var->next) {
    write_file("  .local %.*s.%d\n", var->len, var->name, var->block);
    write_file("  .p2align 3\n");
    write_file("%.*s.%d:\n", var->len, var->name, var->block);
    if (var->offset) {
      write_file("  .long %d\n", var->offset);
    } else {
      write_file("  .zero %d\n", get_sizeof(var->type));
    }
  }
}

// 配列リテラルの生成
void gen_array_literals() {
  for (Array *arr = arrays; arr->next; arr = arr->next) {
    write_file(".L.arr%d:\n", arr->id);
    for (int i = 0; i < arr->len; i++) {
      if (arr->byte == 1) {
        write_file("  .byte %d\n", arr->val[i]);
      } else if (arr->byte == 4) {
        write_file("  .long %d\n", arr->val[i]);
      } else {
        error("invalid array type [INIT]");
      }
    }
  }
}

void gen_rodata_section() {
  write_file("  .rodata\n");
  gen_string_literal();
}

void gen_data_section() {
  write_file("  .data\n");
  gen_global_variables();
  gen_static_variables();
  gen_array_literals();
}
