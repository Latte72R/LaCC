
#include "lacc.h"

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *new_node(NodeKind kind) {
  Node *node = malloc(sizeof(Node));
  node->kind = kind;
  node->endline = FALSE;
  node->val = 0;
  return node;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  node->type = new_type(TY_INT);
  return node;
}

Node *new_deref(Node *lhs) {
  Node *node = new_node(ND_DEREF);
  node->lhs = lhs;
  node->type = lhs->type->ptr_to;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  node->type = lhs->type;
  return node;
}

// 共通の変数作成処理
LVar *new_lvar(Token *tok, Type *type, int is_static, int is_extern) {
  LVar *lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  lvar->ext = is_extern;
  lvar->is_static = is_static;
  lvar->type = type;
  return lvar;
}
