
#include "lacc.h"

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
      printf("  lea rax, %.*s.%d[rip]\n", node->var->len, node->var->name, node->var->block);
    } else {
      printf("  mov rax, rbp\n");
      printf("  sub rax, %d\n", node->var->offset);
    }
    printf("  push rax\n");
  } else if (node->kind == ND_GVAR) {
    printf("  lea rax, %.*s[rip]\n", node->var->len, node->var->name);
    printf("  push rax\n");
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
  } else {
    error("invalid lvalue, node->kind: %d\n", node->kind);
  }
}

void asm_memcpy(Node *lhs, Node *rhs) {
  printf("  pop rdi\n");
  printf("  pop rsi\n");
  int size = get_sizeof(rhs->type);
  int offset = 0;
  while (size > 0) {
    if (size >= 8) {
      printf("  mov rax, QWORD PTR [rdi + %d]\n", offset);
      printf("  mov QWORD PTR [rsi + %d], rax\n", offset);
      size -= 8;
      offset += 8;
    } else if (size >= 4) {
      printf("  mov eax, DWORD PTR [rdi + %d]\n", offset);
      printf("  mov DWORD PTR [rsi + %d], eax\n", offset);
      size -= 4;
      offset += 4;
    } else if (size >= 2) {
      printf("  mov ax, WORD PTR [rdi + %d]\n", offset);
      printf("  mov WORD PTR [rsi + %d], ax\n", offset);
      size -= 2;
      offset += 2;
    } else {
      printf("  mov al, BYTE PTR [rdi + %d]\n", offset);
      printf("  mov BYTE PTR [rsi + %d], al\n", offset);
      size -= 1;
      offset += 1;
    }
  }
  printf("  push rdi\n");
}

void gen(Node *node) {
  if (node->kind == ND_NUM) {
    if (!node->endline)
      printf("  push %d\n", node->val);
    return;
  } else if (node->kind == ND_STRING) {
    printf("  lea rax, [rip + .L.str%d]\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ARRAY) {
    printf("  lea rax, [rip + .L.arr%d]\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if ((node->kind == ND_LVAR) || (node->kind == ND_GVAR)) {
    gen_lval(node);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movsx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR || node->type->ty == TY_ARGARR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_LVAR]");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ADDR) {
    gen_lval(node->lhs);
    return;
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movsx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR || node->type->ty == TY_ARGARR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_DEREF]");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_NOT) {
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  sete al\n");
    printf("  movsx rax, al\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_BITNOT) {
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  not rax\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ASSIGN) {
    if (node->val) {
      gen_lval(node->lhs);
      gen(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      printf("  pop rax\n");
      if (!node->endline)
        printf("  push rax\n");
      return;
    } else if (node->lhs->type->ty == TY_STRUCT) {
      gen_lval(node->lhs);
      gen_lval(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      printf("  pop rax\n");
      if (!node->endline)
        printf("  push rax\n");
      return;
    } else {
      gen_lval(node->lhs);
      gen(node->rhs);
      printf("  pop rdi\n");
      printf("  pop rax\n");
      if (node->lhs->type->ty == TY_INT) {
        printf("  mov DWORD PTR [rax], edi\n");
      } else if (node->lhs->type->ty == TY_CHAR) {
        printf("  mov BYTE PTR [rax], dil\n");
      } else if (node->lhs->type->ty == TY_PTR) {
        printf("  mov QWORD PTR [rax], rdi\n");
      } else {
        error("invalid type [in ND_ASSIGN]");
      }
      if (!node->endline)
        printf("  push rdi\n");
      return;
    }
  } else if (node->kind == ND_POSTINC) {
    gen_lval(node->lhs);
    if (!node->endline) {
      printf("  pop rax\n");
      if (node->type->ty == TY_INT) {
        printf("  movsxd rdi, DWORD PTR [rax]\n");
      } else if (node->type->ty == TY_CHAR) {
        printf("  movsx rdi, BYTE PTR [rax]\n");
      } else if (node->type->ty == TY_PTR) {
        printf("  mov rdi, QWORD PTR [rax]\n");
      } else {
        error("invalid type [in ND_POSTINC]");
      }
      printf("  push rdi\n");
      printf("  push rax\n");
    }
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (node->lhs->type->ty == TY_INT) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (node->lhs->type->ty == TY_CHAR) {
      printf("  mov BYTE PTR [rax], dil\n");
    } else if (node->lhs->type->ty == TY_PTR) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type [in ND_POSTINC]");
    }
    return;
  } else if (node->kind == ND_RETURN) {
    gen(node->rhs);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return;
  } else if (node->kind == ND_BLOCK) {
    for (int i = 0; node->body[i]->kind != ND_NONE; i++) {
      gen(node->body[i]);
    }
    return;
  } else if (node->kind == ND_IF) {
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    if (node->els) {
      printf("  je .Lelse%d\n", node->id);
      gen(node->then);
      printf("  jmp .Lend%d\n", node->id);
      printf(".Lelse%d:\n", node->id);
      gen(node->els);
      printf(".Lend%d:\n", node->id);
    } else {
      printf("  je .Lend%d\n", node->id);
      gen(node->then);
      printf(".Lend%d:\n", node->id);
    }
    return;
  } else if (node->kind == ND_WHILE) {
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_DOWHILE) {
    printf(".Lbegin%d:\n", node->id);
    gen(node->then);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    printf(".Lstep%d:\n", node->id);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_FOR) {
    gen(node->init);
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    gen(node->step);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_BREAK) {
    printf("  jmp .Lend%d\n", node->id);
    return;
  } else if (node->kind == ND_CONTINUE) {
    printf("  jmp .Lstep%d\n", node->id);
    return;
  } else if (node->kind == ND_FUNCDEF) {
    if (node->fn->is_static) {
      printf("  .local %.*s\n", node->fn->len, node->fn->name);
    } else {
      printf("  .globl %.*s\n", node->fn->len, node->fn->name);
    }
    printf("  .p2align 4\n");
    printf("%.*s:\n", node->fn->len, node->fn->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    int offset;
    if (node->fn->offset % 8) {
      offset = (node->fn->offset / 8 + 1) * 8;
    } else {
      offset = node->fn->offset;
    }
    printf("  sub rsp, %d\n", offset);
    for (int i = 0; i < node->val; i++) {
      gen_lval(node->args[i]);
      printf("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        printf("  mov DWORD PTR [rax], %s\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        printf("  mov BYTE PTR [rax], %s\n", regs1(i));
      } else if (node->args[i]->type->ty == TY_PTR || node->args[i]->type->ty == TY_ARGARR) {
        printf("  mov QWORD PTR [rax], %s\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCDEF]");
      }
    }
    gen(node->lhs);
    if (node->fn->type->ty == TY_VOID || startswith(node->fn->name, "main") && node->fn->len == 4) {
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
    }
    return;
  } else if (node->kind == ND_FUNCALL) {
    for (int i = 0; i < node->val; i++) {
      gen(node->args[i]);
    }
    for (int i = node->val - 1; i >= 0; i--) {
      printf("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        printf("  mov %s, eax\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        printf("  movsx %s, al\n", regs4(i));
      } else if (is_ptr_or_arr(node->args[i]->type)) {
        printf("  mov %s, rax\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCALL]");
      }
    }
    // Align stack
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je .Laligned%d\n", node->id);
    printf("  sub rsp, 8\n");
    printf("  jmp .Lfixup%d\n", node->id);
    printf(".Laligned%d:\n", node->id);
    printf("  mov rax, 0\n");
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  jmp .Lend%d\n", node->id);
    printf(".Lfixup%d:\n", node->id);
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_AND) {
    gen(node->lhs);
    printf("  pop rdi\n");
    printf("  cmp rdi, 0\n");
    printf("  mov rax, 0\n");
    printf("  je .Llogical%d\n", node->id);
    gen(node->rhs);
    printf("  pop rdi\n");
    printf("  cmp rdi, 0\n");
    printf("  mov rax, 0\n");
    printf("  je .Llogical%d\n", node->id);
    printf("  mov rax, 1\n");
    printf(".Llogical%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_OR) {
    gen(node->lhs);
    printf("  pop rdi\n");
    printf("  cmp rdi, 0\n");
    printf("  mov rax, 1\n");
    printf("  jne .Llogical%d\n", node->id);
    gen(node->rhs);
    printf("  pop rdi\n");
    printf("  cmp rdi, 0\n");
    printf("  mov rax, 1\n");
    printf("  jne .Llogical%d\n", node->id);
    printf("  mov rax, 0\n");
    printf(".Llogical%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if ((node->kind == ND_GLBDEC) || (node->kind == ND_VARDEC) || (node->kind == ND_EXTERN) ||
             (node->kind == ND_TYPEDEF) || (node->kind == ND_ENUM) || (node->kind == ND_STRUCT) ||
             (node->kind == ND_TYPE || node->kind == ND_NONE)) {
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  if (node->kind == ND_ADD) {
    printf("  add rax, rdi\n");
  } else if (node->kind == ND_SUB) {
    printf("  sub rax, rdi\n");
  } else if (node->kind == ND_MUL) {
    printf("  imul rax, rdi\n");
  } else if (node->kind == ND_DIV) {
    printf("  cqo\n");
    printf("  idiv rdi\n");
  } else if (node->kind == ND_MOD) {
    printf("  cqo\n");
    printf("  idiv rdi\n");
    printf("  mov rax, rdx\n");
  } else if (node->kind == ND_EQ) {
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movsx rax, al\n");
  } else if (node->kind == ND_NE) {
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movsx rax, al\n");
  } else if (node->kind == ND_LT) {
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movsx rax, al\n");
  } else if (node->kind == ND_LE) {
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movsx rax, al\n");
  } else if (node->kind == ND_BITAND) {
    printf("  and rax, rdi\n");
  } else if (node->kind == ND_BITOR) {
    printf("  or rax, rdi\n");
  } else if (node->kind == ND_BITXOR) {
    printf("  xor rax, rdi\n");
  } else if (node->kind == ND_SHL) {
    // シフト量は CL レジスタで指定する
    printf("  mov rcx, rdi\n");
    printf("  shl rax, cl\n");
  } else if (node->kind == ND_SHR) {
    // シフト量は CL レジスタで指定する
    printf("  mov rcx, rdi\n");
    printf("  sar rax, cl\n");
  } else {
    error("invalid node kind");
  }

  if (!node->endline)
    printf("  push rax\n");
}
