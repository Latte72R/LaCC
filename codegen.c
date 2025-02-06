
#include "9cc.h"

//
// Code generator
//

const char *regs[4] = {"rdi", "rsi", "rdx", "rcx"};

void gen_lval(Node *node) {
  if (node->kind != ND_LVAR)
    error("代入の左辺値が変数ではありません");

  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->offset);
  printf("  push rax\n");
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
    printf("  mov rax, [rax]\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_ADDR:
    gen_lval(node->lhs);
    return;
  case ND_DEREF:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  case ND_ASSIGN:
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
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
    gen(node->inc);
    printf("  pop rax\n");
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  case ND_FUNCDEF:
    printf("%.*s:\n", node->val, node->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", node->fn->locals->offset);
    for (int i = 0; i < 4 && node->args[i]; i++) {
      gen_lval(node->args[i]);
      printf("  pop rax\n");
      printf("  mov [rax], %s\n", regs[i]);
    }
    gen(node->body);
    return;
  case ND_FUNCALL:
    for (int i = 0; i < 4 && node->args[i]; i++) {
      gen(node->args[i]);
      printf("  pop rax\n");
      printf("  mov %s, rax\n", regs[i]);
    }
    // Align stack
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je .Laligned%d\n", node->id);
    printf("  sub rsp, 8\n");
    printf("  jmp .Lfixup%d\n", node->id);
    printf(".Laligned%d:\n", node->id);
    printf("  call %.*s\n", node->val, node->name);
    printf("  jmp .Lend%d\n", node->id);
    printf(".Lfixup%d:\n", node->id);
    printf("  call %.*s\n", node->val, node->name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
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
  default:
    break;
  }

  if (!node->endline)
    printf("  push rax\n");
}
