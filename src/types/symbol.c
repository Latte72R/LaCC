
#include "lacc.h"

extern Function *functions;
extern Function *current_fn;
extern LVar *locals;
extern LVar *globals;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern TypeTag *type_tags;

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
  lvar->is_extern = is_extern;
  lvar->is_static = is_static;
  lvar->type = type;
  return lvar;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_gvar(Token *tok) {
  for (LVar *var = globals; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structを名前で検索する。見つからなかった場合はNULLを返す。
Object *find_struct(Token *tok) {
  for (Object *var = structs; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// unionを名前で検索する。見つからなかった場合はNULLを返す。
Object *find_union(Token *tok) {
  for (Object *var = unions; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumを名前で検索する。見つからなかった場合はNULLを返す。
Object *find_enum(Token *tok) {
  for (Object *var = enums; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_enum_member(Token *tok) {
  for (Object *enum_ = enums; enum_->next; enum_ = enum_->next) {
    for (LVar *var = enum_->var; var->next; var = var->next)
      if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
        return var;
  }
  return NULL;
}

// structとunionのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_object_member(Object *object, Token *tok) {
  for (LVar *var = object->var; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// TypeTag を名前で検索する。見つからなかった場合はNULLを返す。
TypeTag *find_type_tag(Token *tok) {
  for (TypeTag *var = type_tags; var->next; var = var->next)
    if (var->len == tok->len && !strncmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 関数を名前で検索する。見つからなかった場合はNULLを返す。
Function *find_fn(Token *tok) {
  for (Function *fn = functions; fn->next; fn = fn->next)
    if (fn->len == tok->len && !strncmp(tok->str, fn->name, fn->len))
      return fn;
  return NULL;
}
