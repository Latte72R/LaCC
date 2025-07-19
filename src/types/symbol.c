
#include "lacc.h"

extern char *user_input;
extern Token *token;
extern Node **code;
extern int label_cnt;
extern int array_cnt;
extern int loop_cnt;
extern int variable_cnt;
extern int logical_cnt;
extern int block_cnt;
extern int loop_id;
extern Function *functions;
extern Function *current_fn;
extern LVar *globals;
extern LVar *statics;
extern Struct *structs;
extern StructTag *struct_tags;
extern Enum *enums;
extern LVar *enum_members;
extern String *strings;
extern Array *arrays;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = current_fn->locals; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_gvar(Token *tok) {
  for (LVar *var = globals; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structを名前で検索する。見つからなかった場合はNULLを返す。
Struct *find_struct(Token *tok) {
  for (Struct *var = structs; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumを名前で検索する。見つからなかった場合はNULLを返す。
Enum *find_enum(Token *tok) {
  for (Enum *var = enums; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_enum_member(Token *tok) {
  for (LVar *var = enum_members; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_struct_member(Struct *struct_, Token *tok) {
  for (LVar *var = struct_->var; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// struct_tagを名前で検索する。見つからなかった場合はNULLを返す。
StructTag *find_struct_tag(Token *tok) {
  for (StructTag *var = struct_tags; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 関数を名前で検索する。見つからなかった場合はNULLを返す。
Function *find_fn(Token *tok) {
  for (Function *fn = functions; fn->next; fn = fn->next)
    if (fn->len == tok->len && !memcmp(tok->str, fn->name, fn->len))
      return fn;
  return NULL;
}
