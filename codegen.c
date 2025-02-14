
#include "9cc.h"

//
// Code generator
//

const char *regs[4] = {"di", "si", "dx", "cx"};

void gen_lval(Node *node) {
  if (node->kind == ND_LVAR) {
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
  switch (node->kind) {
  case ND_NUM:
    if (!node->endline)
      printf("  push %d\n", node->val);
    return;
  case ND_LVAR:
    gen_lval(node);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  mov eax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_GVAR:
    gen_lval(node);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  mov eax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_ADDR:
    gen_lval(node->lhs);
    return;
  case ND_DEREF:
    gen(node->lhs);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  mov eax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_NOT:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  sete al\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_ASSIGN:
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (get_type_size(node->lhs->type) == 4) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (get_type_size(node->lhs->type) == 8) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type size [in ND_ASSIGN]");
    }
    if (!node->endline)
      printf("  push rdi\n");
    return;
  case ND_RETURN:
    gen(node->rhs);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return;
  case ND_BLOCK:
    for (int i = 0; node->body[i].kind != ND_NONE; i++) {
      gen(&(node->body[i]));
    }
    return;
  case ND_IF:
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
  case ND_WHILE:
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lstep%d:\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  case ND_FOR:
    gen(node->init);
    printf("  pop rax\n");
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    gen(node->inc);
    printf("  pop rax\n");
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  case ND_BREAK:
    printf("  jmp .Lend%d\n", node->id);
    return;
  case ND_CONTINUE:
    printf("  jmp .Lstep%d\n", node->id);
    return;
  case ND_FUNCDEF:
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
    for (int i = 0; i < 4 && node->args[i]; i++) {
      gen_lval(node->args[i]);
      printf("  pop rax\n");
      if (get_type_size(node->args[i]->type) == 4) {
        printf("  mov DWORD PTR [rax], e%s\n", regs[i]);
      } else if (get_type_size(node->args[i]->type) == 8) {
        printf("  mov QWORD PTR [rax], r%s\n", regs[i]);
      } else {
        error("invalid type size [in ND_FUNCDEF]");
      }
    }
    gen(node->body);
    return;
  case ND_FUNCALL:
    for (int i = 0; i < 4 && node->args[i]; i++) {
      gen(node->args[i]);
    }
    for (int i = 3; i >= 0; i--) {
      if (!node->args[i])
        continue;
      printf("  pop rax\n");
      if (get_type_size(node->args[i]->type) == 4) {
        printf("  mov e%s, eax\n", regs[i]);
      } else if (get_type_size(node->args[i]->type) == 8) {
        printf("  mov r%s, rax\n", regs[i]);
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
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  jmp .Lend%d\n", node->id);
    printf(".Lfixup%d:\n", node->id);
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_GLBDEC:
    return;
  case ND_VARDEC:
    return;
  case ND_EXTERN:
    return;
  default:
    break;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_REM:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    printf("  mov rax, rdx\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzx rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzx rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzx rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzx rax, al\n");
    break;
  case ND_AND:
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  and al, dl\n");
    break;
  case ND_OR:
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  or al, dl\n");
    break;
  default:
    break;
  }

  if (!node->endline)
    printf("  push rax\n");
}
