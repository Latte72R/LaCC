
#include "lacc.h"

extern Node **code;
extern LVar *globals;
extern LVar *statics;
extern String *strings;
extern Array *arrays;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

void gen_lval(Node *node) {
  switch (node->kind) {
  case ND_LVAR:
  case ND_VARDEC:
    if (node->var->is_static) {
      write_file("  lea rax, %.*s.%d[rip]\n", node->var->len, node->var->name, node->var->block);
    } else {
      write_file("  mov rax, rbp\n");
      write_file("  sub rax, %d\n", node->var->offset);
    }
    write_file("  push rax\n");
    break;
  case ND_GVAR:
    write_file("  lea rax, %.*s[rip]\n", node->var->len, node->var->name);
    write_file("  push rax\n");
    break;
  case ND_DEREF:
    gen(node->lhs);
    break;
  default:
    error("invalid lvalue, node->kind: %d\n", node->kind);
  }
}

void asm_memcpy(Node *lhs, Node *rhs) {
  write_file("  pop rdi\n");
  write_file("  pop rsi\n");
  int size = get_sizeof(lhs->type);
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
    if (label->len == len && !strncmp(label->name, name, len)) {
      return label;
    }
  }
  error_at(name, "undefined label: %.*s [in find_label]", len, name);
  return NULL; // Unreachable, but avoids compiler warning.
}

void gen_expression(Node *node) {
  gen(node->lhs);
  gen(node->rhs);

  write_file("  pop rdi\n");
  write_file("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    write_file("  add rax, rdi\n");
    break;
  case ND_SUB:
    write_file("  sub rax, rdi\n");
    break;
  case ND_MUL:
    write_file("  imul rax, rdi\n");
    break;
  case ND_DIV:
    write_file("  cqo\n");
    write_file("  idiv rdi\n");
    break;
  case ND_MOD:
    write_file("  cqo\n");
    write_file("  idiv rdi\n");
    write_file("  mov rax, rdx\n");
    break;
  case ND_EQ:
    write_file("  cmp rax, rdi\n");
    write_file("  sete al\n");
    write_file("  movsx rax, al\n");
    break;
  case ND_NE:
    write_file("  cmp rax, rdi\n");
    write_file("  setne al\n");
    write_file("  movsx rax, al\n");
    break;
  case ND_LT:
    write_file("  cmp rax, rdi\n");
    write_file("  setl al\n");
    write_file("  movsx rax, al\n");
    break;
  case ND_LE:
    write_file("  cmp rax, rdi\n");
    write_file("  setle al\n");
    write_file("  movsx rax, al\n");
    break;
  case ND_BITAND:
    write_file("  and rax, rdi\n");
    break;
  case ND_BITOR:
    write_file("  or rax, rdi\n");
    break;
  case ND_BITXOR:
    write_file("  xor rax, rdi\n");
    break;
  case ND_SHL:
    // シフト量は CL レジスタで指定する
    write_file("  mov rcx, rdi\n");
    write_file("  shl rax, cl\n");
    break;
  case ND_SHR:
    // シフト量は CL レジスタで指定する
    write_file("  mov rcx, rdi\n");
    write_file("  sar rax, cl\n");
    break;
  default:
    error("invalid node kind");
  }

  if (!node->endline)
    write_file("  push rax\n");
}

void gen(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    if (!node->endline)
      write_file("  push %d\n", node->val);
    break;
  case ND_STRING:
    write_file("  lea rax, [rip + .L.str%d]\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_ARRAY:
    write_file("  lea rax, [rip + .L.arr%d]\n", node->id);
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_LVAR:
  case ND_GVAR:
    gen_lval(node);
    write_file("  pop rax\n");
    switch (node->type->ty) {
    case TY_INT:
      write_file("  movsxd rax, DWORD PTR [rax]\n");
      break;
    case TY_CHAR:
      write_file("  movsx rax, BYTE PTR [rax]\n");
      break;
    case TY_PTR:
    case TY_ARGARR:
      write_file("  mov rax, QWORD PTR [rax]\n");
      break;
    case TY_ARR:
      break;
    default:
      error("invalid type [in ND_LVAR]");
    }
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_ADDR:
    gen_lval(node->lhs);
    break;
  case ND_DEREF:
    gen(node->lhs);
    write_file("  pop rax\n");
    switch (node->type->ty) {
    case TY_INT:
      write_file("  movsxd rax, DWORD PTR [rax]\n");
      break;
    case TY_CHAR:
      write_file("  movsx rax, BYTE PTR [rax]\n");
      break;
    case TY_PTR:
    case TY_ARGARR:
      write_file("  mov rax, QWORD PTR [rax]\n");
      break;
    case TY_ARR:
      break;
    default:
      error("invalid type [in ND_DEREF]");
    }
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_NOT:
    gen(node->lhs);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  sete al\n");
    write_file("  movsx rax, al\n");
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_BITNOT:
    gen(node->lhs);
    write_file("  pop rax\n");
    write_file("  not rax\n");
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_ASSIGN:
    if (node->val) {
      gen_lval(node->lhs);
      gen(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      write_file("  pop rax\n");
      if (!node->endline)
        write_file("  push rax\n");
      break;
    } else if (node->lhs->type->ty == TY_STRUCT || node->lhs->type->ty == TY_UNION) {
      gen_lval(node->lhs);
      gen_lval(node->rhs);
      asm_memcpy(node->lhs, node->rhs);
      write_file("  pop rax\n");
      if (!node->endline)
        write_file("  push rax\n");
      break;
    } else {
      gen_lval(node->lhs);
      gen(node->rhs);
      write_file("  pop rdi\n");
      write_file("  pop rax\n");
      switch (node->lhs->type->ty) {
      case TY_INT:
        write_file("  mov DWORD PTR [rax], edi\n");
        break;
      case TY_CHAR:
        write_file("  mov BYTE PTR [rax], dil\n");
        break;
      case TY_PTR:
        write_file("  mov QWORD PTR [rax], rdi\n");
        break;
      default:
        error("invalid type [in ND_ASSIGN]");
      }
      if (!node->endline)
        write_file("  push rdi\n");
      break;
    }
  case ND_POSTINC:
    gen_lval(node->lhs);
    if (!node->endline) {
      write_file("  pop rax\n");
      switch (node->type->ty) {
      case TY_INT:
        write_file("  movsxd rdi, DWORD PTR [rax]\n");
        break;
      case TY_CHAR:
        write_file("  movsx rdi, BYTE PTR [rax]\n");
        break;
      case TY_PTR:
        write_file("  mov rdi, QWORD PTR [rax]\n");
        break;
      default:
        error("invalid type [in ND_POSTINC]");
      }
      write_file("  push rdi\n");
      write_file("  push rax\n");
    }
    gen(node->rhs);

    write_file("  pop rdi\n");
    write_file("  pop rax\n");
    switch (node->lhs->type->ty) {
    case TY_INT:
      write_file("  mov DWORD PTR [rax], edi\n");
      break;
    case TY_CHAR:
      write_file("  mov BYTE PTR [rax], dil\n");
      break;
    case TY_PTR:
      write_file("  mov QWORD PTR [rax], rdi\n");
      break;
    default:
      error("invalid type [in ND_POSTINC]");
    }
    break;
  case ND_RETURN:
    gen(node->rhs);
    write_file("  pop rax\n");
    write_file("  mov rsp, rbp\n");
    write_file("  pop rbp\n");
    write_file("  ret\n");
    break;
  case ND_BLOCK:
    for (int i = 0; node->body[i]->kind != ND_NONE; i++) {
      gen(node->body[i]);
    }
    break;
  case ND_IF:
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
    break;
  case ND_WHILE:
    write_file(".Lbegin%d:\n", node->id);
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lend%d\n", node->id);
    gen(node->then);
    write_file(".Lstep%d:\n", node->id);
    write_file("  jmp .Lbegin%d\n", node->id);
    write_file(".Lend%d:\n", node->id);
    break;
  case ND_DOWHILE:
    write_file(".Lbegin%d:\n", node->id);
    gen(node->then);
    gen(node->cond);
    write_file("  pop rax\n");
    write_file("  cmp rax, 0\n");
    write_file("  je .Lend%d\n", node->id);
    write_file(".Lstep%d:\n", node->id);
    write_file("  jmp .Lbegin%d\n", node->id);
    write_file(".Lend%d:\n", node->id);
    break;
  case ND_FOR:
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
    break;
  case ND_BREAK:
    write_file("  jmp .Lend%d\n", node->id);
    break;
  case ND_CONTINUE:
    write_file("  jmp .Lstep%d\n", node->id);
    break;
  case ND_GOTO: {
    Label *label = find_label(node->fn, node->label->name, node->label->len);
    write_file("  jmp .Llabel%d\n", label->id);
    break;
  }
  case ND_LABEL:
    write_file(".Llabel%d:\n", node->label->id);
    break;
  case ND_SWITCH:
    gen(node->cond);
    write_file("  pop rax\n");
    for (int i = 0; i < node->case_cnt; i++) {
      int n = node->cases[i];
      write_file("  cmp rax, %d\n", n);
      if (n < 0) {
        write_file("  je .Lcase%d__%d\n", node->id, -n);
      } else {
        write_file("  je .Lcase%d_%d\n", node->id, n);
      }
    }
    if (node->has_default) {
      write_file("  jmp .Ldefault%d\n", node->id);
    } else {
      write_file("  jmp .Lend%d\n", node->id);
    }
    gen(node->then);
    write_file(".Lend%d:\n", node->id);
    break;
  case ND_CASE: {
    int n = node->val;
    if (n < 0) {
      write_file(".Lcase%d__%d:\n", node->id, -n);
    } else {
      write_file(".Lcase%d_%d:\n", node->id, n);
    }
    break;
  }
  case ND_DEFAULT:
    write_file(".Ldefault%d:\n", node->id);
    break;
  case ND_TYPECAST:
    gen(node->lhs);
    write_file("  pop rax\n");
    switch (node->lhs->type->ty) {
    case TY_INT:
      write_file("  movsxd rax, eax\n");
      break;
    case TY_CHAR:
      write_file("  movsx rax, al\n");
      break;
    case TY_PTR:
    case TY_ARR:
    case TY_ARGARR:
      // No conversion needed for pointers
      break;
    default:
      error("invalid type [in ND_TYPECAST]");
    }
    if (!node->endline)
      write_file("  push rax\n");
    break;
  case ND_FUNCDEF:
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
      switch (node->args[i]->type->ty) {
      case TY_INT:
        write_file("  mov DWORD PTR [rax], %s\n", regs4(i));
        break;
      case TY_CHAR:
        write_file("  mov BYTE PTR [rax], %s\n", regs1(i));
        break;
      case TY_PTR:
      case TY_ARGARR:
        write_file("  mov QWORD PTR [rax], %s\n", regs8(i));
        break;
      default:
        error("invalid type [in ND_FUNCDEF]");
      }
    }
    gen(node->lhs);
    if (node->fn->type->ty == TY_VOID || !strncmp(node->fn->name, "main", 4)) {
      write_file("  mov rax, 0\n");
      write_file("  mov rsp, rbp\n");
      write_file("  pop rbp\n");
      write_file("  ret\n");
    }
    break;
  case ND_FUNCALL:
    for (int i = 0; i < node->val; i++) {
      gen(node->args[i]);
    }
    for (int i = node->val - 1; i >= 0; i--) {
      write_file("  pop rax\n");
      switch (node->args[i]->type->ty) {
      case TY_INT:
        write_file("  mov %s, eax\n", regs4(i));
        break;
      case TY_CHAR:
        write_file("  movsx %s, al\n", regs4(i));
        break;
      case TY_PTR:
      case TY_ARR:
      case TY_ARGARR:
        write_file("  mov %s, rax\n", regs8(i));
        break;
      default:
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
    break;
  case ND_AND:
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
    break;
  case ND_OR:
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
    break;
  case ND_GLBDEC:
  case ND_VARDEC:
  case ND_EXTERN:
  case ND_TYPEDEF:
  case ND_TYPE:
  case ND_ENUM:
  case ND_UNION:
  case ND_STRUCT:
  case ND_NONE:
    break;
  default:
    gen_expression(node);
  }
}

void gen_text_section() {
  write_file("  .text\n");
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    gen(code[i]);
  }
}
