
#include "lcc.h"

//
// Code generator
//

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
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  push rax\n");
  } else if (node->kind == ND_GVAR) {
    printf("  lea rax, %.*s[rip]\n", node->var->len, node->var->name);
    printf("  push rax\n");
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
  } else {
    error("invalid lvalue");
  }
}

void gen(Node *node) {
  int i;
  if (node->kind == ND_NUM) {
    if (!node->endline)
      printf("  push %d\n", node->val);
    return;
  } else if (node->kind == ND_STR) {
    printf("  lea rax, [rip + .L.str%d]\n", node->str->label);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if ((node->kind == ND_LVAR) || (node->kind == ND_GVAR)) {
    gen_lval(node);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  mov eax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movzx eax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
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
      printf("  mov eax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movzx eax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_NOT) {
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  sete al\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ASSIGN) {
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (get_type_size(node->lhs->type) == 4) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (get_type_size(node->lhs->type) == 1) {
      printf("  mov BYTE PTR [rax], dil\n");
    } else if (get_type_size(node->lhs->type) == 8) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type size [in ND_ASSIGN]");
    }
    if (!node->endline)
      printf("  push rdi\n");
    return;
  } else if (node->kind == ND_POSTINC) {
    gen_lval(node->lhs);
    if (!node->endline) {
      printf("  pop rax\n");
      if (node->type->ty == TY_INT) {
        printf("  mov edi, DWORD PTR [rax]\n");
      } else if (node->type->ty == TY_CHAR) {
        printf("  movzx edi, BYTE PTR [rax]\n");
      } else if (node->type->ty == TY_PTR) {
        printf("  mov rdi, QWORD PTR [rax]\n");
      }
      printf("  push rdi\n");
      printf("  push rax\n");
    }
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (get_type_size(node->lhs->type) == 4) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (get_type_size(node->lhs->type) == 1) {
      printf("  mov BYTE PTR [rax], dil\n");
    } else if (get_type_size(node->lhs->type) == 8) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type size [in ND_ASSIGN]");
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
    for (i = 0; node->body[i]->kind != ND_NONE; i++) {
      gen(node->body[i]);
    }
    return;
  } else if (node->kind == ND_IF) {
    gen(node->cond);
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
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_FOR) {
    gen(node->init);
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    gen(node->inc);
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
    printf("%.*s:\n", node->fn->len, node->fn->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    int offset;
    if (node->fn->locals->offset % 8) {
      offset = (node->fn->locals->offset / 8 + 1) * 8;
    } else {
      offset = node->fn->locals->offset;
    }
    printf("  sub rsp, %d\n", offset);
    for (i = 0; i < 6 && node->args[i]; i++) {
      gen_lval(node->args[i]);
      printf("  pop rax\n");
      if (get_type_size(node->args[i]->type) == 4) {
        printf("  mov DWORD PTR [rax], %s\n", regs4(i));
      } else if (get_type_size(node->args[i]->type) == 1) {
        printf("  mov BYTE PTR [rax], %s\n", regs1(i));
      } else if (get_type_size(node->args[i]->type) == 8) {
        printf("  mov QWORD PTR [rax], %s\n", regs8(i));
      } else {
        error("invalid type size [in ND_FUNCDEF]");
      }
    }
    gen(node->lhs);
    if (node->fn->type->ty == TY_VOID) {
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
    }
    return;
  } else if (node->kind == ND_FUNCALL) {
    for (i = 0; i < 6 && node->args[i]; i++) {
      gen(node->args[i]);
    }
    for (i = 3; i >= 0; i--) {
      if (!node->args[i])
        continue;
      printf("  pop rax\n");
      if (get_type_size(node->args[i]->type) == 4) {
        printf("  mov %s, eax\n", regs4(i));
      } else if (get_type_size(node->args[i]->type) == 1) {
        printf("  movzx %s, al\n", regs4(i));
      } else if (get_type_size(node->args[i]->type) == 8) {
        printf("  mov %s, rax\n", regs8(i));
      } else {
        error("invalid type size [in ND_FUNCALL]");
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
  } else if ((node->kind == ND_GLBDEC) || (node->kind == ND_VARDEC) || (node->kind == ND_EXTERN) ||
             (node->kind == ND_TYPEDEF) || (node->kind == ND_ENUM) || (node->kind == ND_STRUCT) ||
             (node->kind == ND_TYPE)) {
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
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_NE) {
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_LT) {
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_LE) {
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_AND) {
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  and al, dl\n");
  } else if (node->kind == ND_OR) {
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  or al, dl\n");
  } else {
    error("invalid node kind");
  }

  if (!node->endline)
    printf("  push rax\n");
}
