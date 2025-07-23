
#include "lacc.h"

extern Node **code;
extern LVar *globals;
extern LVar *statics;
extern String *strings;
extern Array *arrays;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

char *regs1(int i) {
  if (i == 0)
    return "dil";
  else if (i == 1)
    return "sil";
  else if (i == 2)
    return "dl";
  else if (i == 3)
    return "cl";
  else if (i == 4)
    return "r8b";
  else if (i == 5)
    return "r9b";
  else
    return NULL;
}

char *regs4(int i) {
  if (i == 0)
    return "edi";
  else if (i == 1)
    return "esi";
  else if (i == 2)
    return "edx";
  else if (i == 3)
    return "ecx";
  else if (i == 4)
    return "r8d";
  else if (i == 5)
    return "r9d";
  else
    return NULL;
}

char *regs8(int i) {
  if (i == 0)
    return "rdi";
  else if (i == 1)
    return "rsi";
  else if (i == 2)
    return "rdx";
  else if (i == 3)
    return "rcx";
  else if (i == 4)
    return "r8";
  else if (i == 5)
    return "r9";
  else
    return NULL;
}

void gen_lval(Node *node) {
  if (node->kind == ND_LVAR || node->kind == ND_VARDEC) {
    if (node->var->is_static) {
      write_file("  lea rax, %.*s.%d[rip]\n", node->var->len, node->var->name, node->var->block);
    } else {
      write_file("  mov rax, rbp\n");
      write_file("  sub rax, %d\n", node->var->offset);
    }
    write_file("  push rax\n");
  } else if (node->kind == ND_GVAR) {
    write_file("  lea rax, %.*s[rip]\n", node->var->len, node->var->name);
    write_file("  push rax\n");
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
  } else {
    error("invalid lvalue, node->kind: %d\n", node->kind);
  }
}

void asm_memcpy(Node *lhs, Node *rhs) {
  write_file("  pop rdi\n");
  write_file("  pop rsi\n");
  int size = get_sizeof(rhs->type);
  int offset = 0;
  while (size > 0) {
    if (size >= 8) {
      write_file("  mov rax, QWORD PTR [rdi + %d]\n", offset);
      write_file("  mov QWORD PTR [rsi + %d], rax\n", offset);
      size -= 8;
      offset += 8;
    } else if (size >= 4) {
      write_file("  mov eax, DWORD PTR [rdi + %d]\n", offset);
      write_file("  mov DWORD PTR [rsi + %d], eax\n", offset);
      size -= 4;
      offset += 4;
    } else if (size >= 2) {
      write_file("  mov ax, WORD PTR [rdi + %d]\n", offset);
      write_file("  mov WORD PTR [rsi + %d], ax\n", offset);
      size -= 2;
      offset += 2;
    } else {
      write_file("  mov al, BYTE PTR [rdi + %d]\n", offset);
      write_file("  mov BYTE PTR [rsi + %d], al\n", offset);
      size -= 1;
      offset += 1;
    }
  }
  write_file("  push rdi\n");
}

Label *find_label(Function *fn, char *name, int len) {
  for (Label *label = fn->labels; label->next; label = label->next) {
    if (label->len == len && !memcmp(label->name, name, len)) {
      return label;
    }
  }
  error_at(name, "undefined label: %.*s [in find_label]", len, name);
  return NULL; // Unreachable, but avoids compiler warning.
}

void gen(Node *node) {
  if (node->kind == ND_NUM) {
    if (!node->endline)
      write_file("  push %d\n", node->val);
    return;
  } else if (node->kind == ND_STRING) {
    write_file("  lea rax, [rip + .L.str%d]\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_ARRAY) {
    write_file("  lea rax, [rip + .L.arr%d]\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if ((node->kind == ND_LVAR) || (node->kind == ND_GVAR)) {
    gen_lval(node);
    write_file("  pop rax\n");
    if (node->type->ty == TY_INT) {
      write_file("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      write_file("  movsx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR || node->type->ty == TY_ARGARR) {
      write_file("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_LVAR]");
    }
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_ADDR) {
    gen_lval(node->lhs);
    return;
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
    write_file("  pop rax\n");
    if (node->type->ty == TY_INT) {
      write_file("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      write_file("  movsx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR || node->type->ty == TY_ARGARR) {
      write_file("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_DEREF]");
    }
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_NOT) {
    gen(node->lhs);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  sete al\n");
    write_file("  movsx rax, al\n");
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_BITNOT) {
    gen(node->lhs);
    write_file("  pop rax\n");
    write_file("  not rax\n");
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_ASSIGN) {
    if (node->val) {
      gen_lval(node->lhs);
      gen(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      write_file("  pop rax\n");
      if (!node->endline)
        write_file("  push rax\n");
      return;
    } else if (node->lhs->type->ty == TY_STRUCT) {
      gen_lval(node->lhs);
      gen_lval(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      write_file("  pop rax\n");
      if (!node->endline)
        write_file("  push rax\n");
      return;
    } else {
      gen_lval(node->lhs);
      gen(node->rhs);
      write_file("  pop rdi\n");
      write_file("  pop rax\n");
      if (node->lhs->type->ty == TY_INT) {
        write_file("  mov DWORD PTR [rax], edi\n");
      } else if (node->lhs->type->ty == TY_CHAR) {
        write_file("  mov BYTE PTR [rax], dil\n");
      } else if (node->lhs->type->ty == TY_PTR) {
        write_file("  mov QWORD PTR [rax], rdi\n");
      } else {
        error("invalid type [in ND_ASSIGN]");
      }
      if (!node->endline)
        write_file("  push rdi\n");
      return;
    }
  } else if (node->kind == ND_POSTINC) {
    gen_lval(node->lhs);
    if (!node->endline) {
      write_file("  pop rax\n");
      if (node->type->ty == TY_INT) {
        write_file("  movsxd rdi, DWORD PTR [rax]\n");
      } else if (node->type->ty == TY_CHAR) {
        write_file("  movsx rdi, BYTE PTR [rax]\n");
      } else if (node->type->ty == TY_PTR) {
        write_file("  mov rdi, QWORD PTR [rax]\n");
      } else {
        error("invalid type [in ND_POSTINC]");
      }
      write_file("  push rdi\n");
      write_file("  push rax\n");
    }
    gen(node->rhs);

    write_file("  pop rdi\n");
    write_file("  pop rax\n");
    if (node->lhs->type->ty == TY_INT) {
      write_file("  mov DWORD PTR [rax], edi\n");
    } else if (node->lhs->type->ty == TY_CHAR) {
      write_file("  mov BYTE PTR [rax], dil\n");
    } else if (node->lhs->type->ty == TY_PTR) {
      write_file("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type [in ND_POSTINC]");
    }
    return;
  } else if (node->kind == ND_RETURN) {
    gen(node->rhs);
    write_file("  pop rax\n");
    write_file("  mov rsp, rbp\n");
    write_file("  pop rbp\n");
    write_file("  ret\n");
    return;
  } else if (node->kind == ND_BLOCK) {
    for (int i = 0; node->body[i]->kind != ND_NONE; i++) {
      gen(node->body[i]);
    }
    return;
  } else if (node->kind == ND_IF) {
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    if (node->els) {
      write_file("  je .Lelse%d\n", node->id);
      gen(node->then);
      write_file("  jmp .Lend%d\n", node->id);
      write_file(".Lelse%d:\n", node->id);
      gen(node->els);
      write_file(".Lend%d:\n", node->id);
    } else {
      write_file("  je .Lend%d\n", node->id);
      gen(node->then);
      write_file(".Lend%d:\n", node->id);
    }
    return;
  } else if (node->kind == ND_WHILE) {
    write_file(".Lbegin%d:\n", node->id);
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lend%d\n", node->id);
    gen(node->then);
    write_file(".Lstep%d:\n", node->id);
    write_file("  jmp .Lbegin%d\n", node->id);
    write_file(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_DOWHILE) {
    write_file(".Lbegin%d:\n", node->id);
    gen(node->then);
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lend%d\n", node->id);
    write_file(".Lstep%d:\n", node->id);
    write_file("  jmp .Lbegin%d\n", node->id);
    write_file(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_FOR) {
    gen(node->init);
    write_file(".Lbegin%d:\n", node->id);
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lend%d\n", node->id);
    gen(node->then);
    write_file(".Lstep%d:\n", node->id);
    gen(node->step);
    write_file("  jmp .Lbegin%d\n", node->id);
    write_file(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_BREAK) {
    write_file("  jmp .Lend%d\n", node->id);
    return;
  } else if (node->kind == ND_CONTINUE) {
    write_file("  jmp .Lstep%d\n", node->id);
    return;
  } else if (node->kind == ND_GOTO) {
    Label *label = find_label(node->fn, node->label->name, node->label->len);
    write_file("  jmp .L%d\n", label->id);
    return;
  } else if (node->kind == ND_LABEL) {
    write_file(".L%d:\n", node->label->id);
    return;
  } else if (node->kind == ND_FUNCDEF) {
    if (node->fn->is_static) {
      write_file("  .local %.*s\n", node->fn->len, node->fn->name);
    } else {
      write_file("  .globl %.*s\n", node->fn->len, node->fn->name);
    }
    write_file("  .p2align 4\n");
    write_file("%.*s:\n", node->fn->len, node->fn->name);
    write_file("  push rbp\n");
    write_file("  mov rbp, rsp\n");
    int offset;
    if (node->fn->offset % 8) {
      offset = (node->fn->offset / 8 + 1) * 8;
    } else {
      offset = node->fn->offset;
    }
    write_file("  sub rsp, %d\n", offset);
    for (int i = 0; i < node->val; i++) {
      gen_lval(node->args[i]);
      write_file("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        write_file("  mov DWORD PTR [rax], %s\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        write_file("  mov BYTE PTR [rax], %s\n", regs1(i));
      } else if (node->args[i]->type->ty == TY_PTR || node->args[i]->type->ty == TY_ARGARR) {
        write_file("  mov QWORD PTR [rax], %s\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCDEF]");
      }
    }
    gen(node->lhs);
    if (node->fn->type->ty == TY_VOID || startswith(node->fn->name, "main") && node->fn->len == 4) {
      write_file("  mov rax, 0\n");
      write_file("  mov rsp, rbp\n");
      write_file("  pop rbp\n");
      write_file("  ret\n");
    }
    return;
  } else if (node->kind == ND_FUNCALL) {
    for (int i = 0; i < node->val; i++) {
      gen(node->args[i]);
    }
    for (int i = node->val - 1; i >= 0; i--) {
      write_file("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        write_file("  mov %s, eax\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        write_file("  movsx %s, al\n", regs4(i));
      } else if (is_ptr_or_arr(node->args[i]->type)) {
        write_file("  mov %s, rax\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCALL]");
      }
    }
    // Align stack
    write_file("  mov rax, rsp\n");
    write_file("  and rax, 0xF\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Laligned%d\n", node->id);
    write_file("  sub rsp, 8\n");
    write_file("  jmp .Lfixup%d\n", node->id);
    write_file(".Laligned%d:\n", node->id);
    write_file("  mov rax, 0\n");
    write_file("  call %.*s\n", node->fn->len, node->fn->name);
    write_file("  jmp .Lend%d\n", node->id);
    write_file(".Lfixup%d:\n", node->id);
    write_file("  call %.*s\n", node->fn->len, node->fn->name);
    write_file("  add rsp, 8\n");
    write_file(".Lend%d:\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_AND) {
    gen(node->lhs);
    write_file("  pop rdi\n");
    write_file("  cmp rdi, 0\n");
    write_file("  mov rax, 0\n");
    write_file("  je .Llogical%d\n", node->id);
    gen(node->rhs);
    write_file("  pop rdi\n");
    write_file("  cmp rdi, 0\n");
    write_file("  mov rax, 0\n");
    write_file("  je .Llogical%d\n", node->id);
    write_file("  mov rax, 1\n");
    write_file(".Llogical%d:\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if (node->kind == ND_OR) {
    gen(node->lhs);
    write_file("  pop rdi\n");
    write_file("  cmp rdi, 0\n");
    write_file("  mov rax, 1\n");
    write_file("  jne .Llogical%d\n", node->id);
    gen(node->rhs);
    write_file("  pop rdi\n");
    write_file("  cmp rdi, 0\n");
    write_file("  mov rax, 1\n");
    write_file("  jne .Llogical%d\n", node->id);
    write_file("  mov rax, 0\n");
    write_file(".Llogical%d:\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    return;
  } else if ((node->kind == ND_GLBDEC) || (node->kind == ND_VARDEC) || (node->kind == ND_EXTERN) ||
             (node->kind == ND_TYPEDEF) || (node->kind == ND_ENUM) || (node->kind == ND_STRUCT) ||
             (node->kind == ND_TYPE || node->kind == ND_NONE)) {
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  write_file("  pop rdi\n");
  write_file("  pop rax\n");

  if (node->kind == ND_ADD) {
    write_file("  add rax, rdi\n");
  } else if (node->kind == ND_SUB) {
    write_file("  sub rax, rdi\n");
  } else if (node->kind == ND_MUL) {
    write_file("  imul rax, rdi\n");
  } else if (node->kind == ND_DIV) {
    write_file("  cqo\n");
    write_file("  idiv rdi\n");
  } else if (node->kind == ND_MOD) {
    write_file("  cqo\n");
    write_file("  idiv rdi\n");
    write_file("  mov rax, rdx\n");
  } else if (node->kind == ND_EQ) {
    write_file("  cmp rax, rdi\n");
    write_file("  sete al\n");
    write_file("  movsx rax, al\n");
  } else if (node->kind == ND_NE) {
    write_file("  cmp rax, rdi\n");
    write_file("  setne al\n");
    write_file("  movsx rax, al\n");
  } else if (node->kind == ND_LT) {
    write_file("  cmp rax, rdi\n");
    write_file("  setl al\n");
    write_file("  movsx rax, al\n");
  } else if (node->kind == ND_LE) {
    write_file("  cmp rax, rdi\n");
    write_file("  setle al\n");
    write_file("  movsx rax, al\n");
  } else if (node->kind == ND_BITAND) {
    write_file("  and rax, rdi\n");
  } else if (node->kind == ND_BITOR) {
    write_file("  or rax, rdi\n");
  } else if (node->kind == ND_BITXOR) {
    write_file("  xor rax, rdi\n");
  } else if (node->kind == ND_SHL) {
    // シフト量は CL レジスタで指定する
    write_file("  mov rcx, rdi\n");
    write_file("  shl rax, cl\n");
  } else if (node->kind == ND_SHR) {
    // シフト量は CL レジスタで指定する
    write_file("  mov rcx, rdi\n");
    write_file("  sar rax, cl\n");
  } else {
    error("invalid node kind");
  }

  if (!node->endline)
    write_file("  push rax\n");
}

void generate_assembly() { // アセンブリの前半部分を出力
  write_file(".intel_syntax noprefix\n");

  write_file("  .rodata\n");
  // 文字列リテラル
  for (String *str = strings; str->next; str = str->next) {
    write_file(".L.str%d:\n", str->id);
    write_file("  .string \"%.*s\"\n", str->len, str->text);
  }

  write_file("  .data\n");
  // グローバル変数の定義
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

  // 静的な関数内変数の定義
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

  // 配列リテラル
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

  // 先頭の式から順にコード生成
  write_file("  .text\n");
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    gen(code[i]);
  }
}
