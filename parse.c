
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

void free_token() {
  Token *tok = token;
  token = tok->next;
  free(tok);
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = current_fn->locals; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_gver(Token *tok) {
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

// Consumes the current token if it matches `op`.
int consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return FALSE;
  consumed_ptr = token->str;
  free_token();
  return TRUE;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  consumed_ptr = token->str;
  free_token();
  return tok;
}

Type *check_base_type() {
  Token *tok = token;
  Type *type = malloc(sizeof(Type));
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    token = token->next;
  } else {
    type->const_ = FALSE;
  }
  if (token->kind == TK_IDENT) {
    Struct *struct_ = find_struct(token);
    Enum *enum_ = find_enum(token);
    if (struct_) {
      type->ty = TY_STRUCT;
      type->struct_ = struct_;
    } else if (enum_) {
      type->ty = TY_INT;
    } else {
      return NULL;
    }
  } else if (token->kind != TK_TYPE) {
    return NULL;
  } else {
    type->ty = token->ty;
  }
  token = token->next;
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    token = token->next;
  }
  token = tok;
  return type;
}

Type *consume_type() {
  Type *type = malloc(sizeof(Type));
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    free_token();
  } else {
    type->const_ = FALSE;
  }
  if (token->kind == TK_IDENT) {
    Struct *struct_ = find_struct(token);
    Enum *enum_ = find_enum(token);
    if (struct_) {
      type->ty = TY_STRUCT;
      type->struct_ = struct_;
    } else if (enum_) {
      type->ty = TY_INT;
    } else {
      return NULL;
    }
  } else if (token->kind != TK_TYPE) {
    return NULL;
  } else {
    type->ty = token->ty;
  }
  consumed_ptr = token->str;
  free_token();
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    free_token();
  }
  while (consume("*")) {
    Type *ptr = malloc(sizeof(Type));
    ptr->ty = TY_PTR;
    ptr->ptr_to = type;
    type = ptr;
    if (token->kind == TK_CONST) {
      type->const_ = TRUE;
      free_token();
    } else {
      type->const_ = FALSE;
    }
  }
  return type;
}

int is_type(Token *tok) {
  if (tok->kind == TK_TYPE)
    return TRUE;
  if (tok->kind == TK_CONST)
    return TRUE;
  if (tok->kind == TK_IDENT) {
    Struct *struct_ = find_struct(tok);
    if (struct_)
      return TRUE;
    Enum *enum_ = find_enum(tok);
    if (enum_)
      return TRUE;
  }
  return FALSE;
}

// Ensure that the current token is `op`.
void expect(char *op, char *err, char *st) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    error_at(token->str, "expected \"%s\":\n  %s  [in %s statement]", op, err, st);
  free_token();
}

// Ensure that the current token is TK_NUM.
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number but got \"%.*s\" [in expect_number]", token->len, token->str);
  int val = token->val;
  free_token();
  return val;
}

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR || type->ty == TY_ARGARR; }
int is_number(Type *type) { return type->ty == TY_INT || type->ty == TY_CHAR; }

// 予約しているスタック領域のサイズ
int get_sizeof(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_CHAR) {
    return 1;
  } else if (type->ty == TY_PTR) {
    return 8;
  } else if (type->ty == TY_ARR || type->ty == TY_ARGARR) {
    return get_sizeof(type->ptr_to) * type->array_size;
  } else if (type->ty == TY_STRUCT) {
    return type->struct_->size;
  } else {
    error_at(token->str, "invalid type [in get_sizeof]");
    return 0;
  }
}

Type *new_type(TypeKind ty) {
  Type *type = malloc(sizeof(Type));
  type->ty = ty;
  return type;
}

Type *new_type_ptr(Type *ptr_to) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_PTR;
  type->ptr_to = ptr_to;
  type->array_size = 1;
  return type;
}

Type *new_type_arr(Type *ptr_to, int array_size) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_ARR;
  type->ptr_to = ptr_to;
  type->array_size = array_size;
  return type;
}

Type *new_type_struct(Struct *struct_) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_STRUCT;
  type->struct_ = struct_;
  return type;
}

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

Node *function_definition(Token *tok, Type *type, int is_static) {
  Function *fn = find_fn(tok);
  if (fn) {
    // error_at(token->str, "duplicated function name: %.*s", tok->len, tok->str);
  } else {
    fn = malloc(sizeof(Function));
    fn->next = functions;
    functions = fn;
  }
  fn->name = tok->str;
  fn->len = tok->len;
  fn->locals = malloc(sizeof(LVar));
  fn->locals->offset = 0;
  fn->locals->type = new_type(TY_NONE);
  fn->locals->next = NULL;
  fn->type = type;
  fn->offset = 0;
  fn->is_static = is_static;
  fn->labels = malloc(sizeof(Label));
  fn->labels->next = NULL;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->fn = fn;
  int n = 0;
  for (int i = 0; i < 6; i++) {
    type = consume_type();
    if (!type) {
      if (i != 0) {
        error_at(consumed_ptr, "expected a type [in function definition]");
      }
      break;
    } else if (type->ty == TY_VOID) {
      if (i != 0) {
        error_at(consumed_ptr, "void type is only allowed for the first argument [in function definition]");
      }
      break;
    }
    Token *tok_lvar = consume_ident();
    if (!tok_lvar) {
      error_at(consumed_ptr, "expected an identifier [in variable declaration]");
    }
    while (consume("[")) {
      type = new_type_arr(type, expect_number());
      expect("]", "after number", "array declaration");
    }
    if (type->ty == TY_ARR) {
      type->ty = TY_ARGARR;
    }
    Node *nd_lvar = new_node(ND_LVAR);
    node->args[i] = nd_lvar;
    LVar *lvar = malloc(sizeof(LVar));
    lvar->next = fn->locals;
    lvar->name = tok_lvar->str;
    lvar->len = tok_lvar->len;
    lvar->ext = FALSE;
    lvar->is_static = FALSE;
    if (is_ptr_or_arr(type)) {
      lvar->offset = fn->locals->offset + 8;
    } else {
      lvar->offset = fn->locals->offset + get_sizeof(type);
    }
    fn->offset = lvar->offset;
    lvar->type = type;
    nd_lvar->var = lvar;
    nd_lvar->type = type;
    fn->locals = lvar;
    n += 1;
    if (!consume(","))
      break;
  }
  node->val = n;
  expect(")", "after arguments", "function definition");

  if (!(token->kind == TK_RESERVED && !memcmp(token->str, "{", token->len))) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
  } else {
    node->lhs = stmt();
  }
  current_fn = prev_fn;
  return node;
}

Node *local_variable_declaration(Token *tok, Type *type, int is_static) {
  LVar *lvar = find_lvar(tok);
  if (lvar) {
    error_at(tok->str, "duplicated variable name: %.*s [in variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_VARDEC);
  lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  lvar->ext = FALSE;
  lvar->is_static = is_static;
  Type *org_type = type;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "array declaration");
  }
  if (is_static) {
    lvar->block = block_cnt;
    LVar *static_lvar = malloc(sizeof(LVar));
    static_lvar->name = lvar->name;
    static_lvar->len = lvar->len;
    static_lvar->ext = FALSE;
    static_lvar->is_static = TRUE;
    static_lvar->type = type;
    static_lvar->block = lvar->block;
    static_lvar->next = statics;
    statics = static_lvar;
  } else {
    lvar->offset = current_fn->locals->offset + get_sizeof(type);
    if (current_fn->offset < lvar->offset) {
      current_fn->offset = lvar->offset;
    }
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = current_fn->locals;
  current_fn->locals = lvar;

  // 要修正
  if (is_static) {
    if (consume("=")) {
      int sign = 1;
      if (consume("-")) {
        sign = -1;
      } else if (consume("+")) {
        sign = 1;
      }
      lvar->offset = expect_number() * sign;
    } else {
      lvar->offset = 0;
    }
  } else {
    if (consume("=")) {
      if (consume("{")) {
        if (type->ty != TY_ARR) {
          error_at(token->str, "array initializer is only allowed for array type [in variable declaration]");
        }
        Array *array = malloc(sizeof(Array));
        array->next = arrays;
        arrays = array;
        array->id = array_cnt++;
        array->byte = get_sizeof(org_type);
        array->val = NULL;
        int i = 0;
        while (!(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
          array->val = realloc(array->val, array->byte * ++i);
          array->val[i - 1] = expect_number();
          if (!array->val)
            error("realloc failed");
          if (!consume(",")) {
            break;
          }
        }
        array->len = i;
        expect("}", "after array initializer", "variable declaration");
        Node *arr = new_node(ND_ARRAY);
        arr->type = type;
        arr->id = array->id;
        node = new_binary(ND_ASSIGN, node, arr);
        node->type = type;
        node->val = TRUE;
      } else {
        node = new_binary(ND_ASSIGN, node, expr());
        node->type = type;
        if (node->rhs->kind == ND_STRING && node->lhs->type->ty == TY_ARR) {
          node->val = node->lhs->type->ptr_to->ty == TY_CHAR;
        }
      }
    }
  }
  node->endline = TRUE;
  return node;
}

Node *global_variable_declaration(Token *tok, Type *type, int is_static) {
  LVar *lvar = find_gver(tok);
  if (lvar) {
    error_at(token->str, "duplicated variable name: %.*s [in global variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_GLBDEC);
  lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "array declaration");
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->ext = FALSE;
  lvar->next = globals;
  lvar->is_static = is_static;
  globals = lvar;

  // 要修正
  if (consume("=")) {
    int sign = 1;
    if (consume("-")) {
      sign = -1;
    } else if (consume("+")) {
      sign = 1;
    }
    lvar->offset = expect_number() * sign;
  } else {
    lvar->offset = 0;
  }
  node->endline = TRUE;
  return node;
}

Node *vardec_and_funcdef_stmt(int is_static) {
  // 変数宣言または関数定義
  Node *node;
  Type *ch_type = check_base_type();
  Type *type = consume_type();
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
  }
  if (consume("(")) {
    // 関数定義
    if (current_fn->next) {
      error_at(token->str, "nested function is not supported [in function definition]");
    }
    node = function_definition(tok, type, is_static);
  } else if (current_fn->next) {
    // ローカル変数宣言
    node = new_node(ND_BLOCK);
    node->body = malloc(sizeof(Node *));
    int i = 0;
    node->body[i++] = local_variable_declaration(tok, type, is_static);
    while (consume(",")) {
      type = ch_type;
      while (consume("*")) {
        Type *ptr = malloc(sizeof(Type));
        ptr->ty = TY_PTR;
        ptr->ptr_to = type;
        type = ptr;
        if (token->kind == TK_CONST) {
          type->const_ = TRUE;
        }
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len,
                 token->str);
      }
      node->body[i++] = local_variable_declaration(tok, type, is_static);
      node->body = realloc(node->body, sizeof(Node *) * (i + 1));
      if (!node->body)
        error("realloc failed");
    }
    node->body[i] = new_node(ND_NONE);
    expect(";", "after line", "variable declaration");
  } else {
    // グローバル変数宣言
    node = new_node(ND_BLOCK);
    node->body = malloc(sizeof(Node *));
    int i = 0;
    node->body[i++] = global_variable_declaration(tok, type, is_static);
    while (consume(",")) {
      type = ch_type;
      while (consume("*")) {
        Type *ptr = malloc(sizeof(Type));
        ptr->ty = TY_PTR;
        ptr->ptr_to = type;
        type = ptr;
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len,
                 token->str);
      }
      node->body[i++] = global_variable_declaration(tok, type, is_static);
      node->body = realloc(node->body, sizeof(Node *) * (i + 1));
      if (!node->body)
        error("realloc failed");
    }
    node->body[i] = new_node(ND_NONE);
    expect(";", "after line", "variable declaration");
  }
  return node;
}

Node *extern_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gver(tok);
  Node *node = new_node(ND_EXTERN);
  if (lvar) {
    return node;
  }
  lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "extern declaration");
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->ext = TRUE;
  lvar->is_static = FALSE;
  lvar->next = globals;
  globals = lvar;
  node->endline = TRUE;
  return node;
}

Node *struct_stmt() {
  free_token();
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in struct definition]", token->len, token->str);
  }
  Node *node = new_node(ND_STRUCT);
  StructTag *struct_tag = find_struct_tag(tok);
  if (!struct_tag) {
    error_at(tok->str, "unknown tag: %.*s [in struct declaration]", tok->len, tok->str);
  }
  Struct *struct_ = struct_tag->main;
  struct_->var = malloc(sizeof(LVar));
  struct_->var->next = NULL;
  struct_->var->type = new_type(TY_NONE);
  int offset = 0;
  expect("{", "before struct members", "struct");
  while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
    Type *type = consume_type();
    Token *member_tok = consume_ident();
    if (!member_tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in struct declaration]", token->len, token->str);
    }
    Type *org_type = type;
    while (consume("[")) {
      type = new_type_arr(type, expect_number());
      expect("]", "after number", "array declaration");
    }
    LVar *member_var = malloc(sizeof(LVar));
    member_var->name = member_tok->str;
    member_var->len = member_tok->len;
    member_var->type = type;
    member_var->next = struct_->var;
    member_var->ext = FALSE;
    member_var->is_static = FALSE;
    struct_->var = member_var;
    int single_size = get_sizeof(org_type);
    if (offset % single_size != 0) {
      offset += single_size - (offset % single_size);
    }
    member_var->offset = offset;
    offset += get_sizeof(type);
    if (consume(";")) {
      continue;
    } else {
      error_at(token->str, "expected ';' after struct member declaration [in struct declaration]");
    }
  }
  struct_->size = offset;
  expect("}", "after struct members", "struct");
  expect(";", "after struct definition", "struct");
  node->type = new_type_struct(struct_);
  node->endline = TRUE;
  return node;
}

Node *block_stmt() {
  Node *node = new_node(ND_BLOCK);
  LVar *var = current_fn->locals;
  node->body = malloc(sizeof(Node *));
  int i = 0;
  while (!(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
    node->body[i++] = stmt();
    node->body = realloc(node->body, sizeof(Node *) * (i + 1));
    if (!node->body)
      error("realloc failed");
  }
  node->body[i] = new_node(ND_NONE);
  current_fn->locals = var;
  block_cnt++;
  return node;
}

Node *extern_stmt() {
  free_token();
  Type *ch_type = check_base_type();
  Type *type = consume_type();
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in extern declaration]", token->len, token->str);
  }
  Node *node = new_node(ND_BLOCK);
  node->body = malloc(sizeof(Node *));
  int i = 0;
  node->body[i++] = extern_declaration(tok, type);
  while (consume(",")) {
    type = ch_type;
    while (consume("*")) {
      Type *ptr = malloc(sizeof(Type));
      ptr->ty = TY_PTR;
      ptr->ptr_to = type;
      type = ptr;
      if (type->const_) {
        error_at(token->str, "constant pointer is not allowed [in extern declaration]");
      }
    }
    tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
    }
    node->body[i++] = extern_declaration(tok, type);
    node->body = realloc(node->body, sizeof(Node *) * (i + 1));
    if (!node->body)
      error("realloc failed");
  }
  node->body[i] = new_node(ND_NONE);
  node->endline = TRUE;
  return node;
}

Node *goto_stmt() {
  free_token();
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in goto statement]", token->len, token->str);
  }
  Node *node = new_node(ND_GOTO);
  node->label = malloc(sizeof(Label));
  node->label->name = tok->str;
  node->label->len = tok->len;
  node->fn = current_fn;
  node->endline = TRUE;
  return node;
}

Node *label_stmt() {
  Label *label = malloc(sizeof(Label));
  label->name = token->str;
  label->len = token->len;
  label->id = label_cnt++;
  label->next = current_fn->labels;
  current_fn->labels = label;
  Node *node = new_node(ND_LABEL);
  node->label = label;
  free_token();
  node->endline = TRUE;
  return node;
}

Node *typedef_stmt() {
  free_token();
  Node *node;
  if (token->kind == TK_STRUCT) {
    free_token();
    node = new_node(ND_TYPEDEF);
    Token *tok1 = consume_ident();
    if (!tok1) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
    }
    Token *tok2 = consume_ident();
    if (!tok2) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
    }
    if (find_struct_tag(tok1)) {
      error_at(tok1->str, "duplicated tag name: %.*s [in typedef]", tok1->len, tok1->str);
    }
    if (find_struct(tok2)) {
      error_at(tok2->str, "duplicated struct name: %.*s [in typedef]", tok2->len, tok2->str);
    }
    Struct *var = malloc(sizeof(Struct));
    var->name = tok2->str;
    var->len = tok2->len;
    var->next = structs;
    structs = var;
    StructTag *tag = malloc(sizeof(StructTag));
    tag->name = tok1->str;
    tag->len = tok1->len;
    tag->main = var;
    tag->next = struct_tags;
    struct_tags = tag;
  } else if (token->kind == TK_ENUM) {
    free_token();
    node = new_node(ND_TYPEDEF);
    int offset = 0;
    expect("{", "before enum members", "enum");
    while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
      Token *member_tok = consume_ident();
      if (!member_tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
      }
      LVar *member_var = malloc(sizeof(LVar));
      member_var->name = member_tok->str;
      member_var->len = member_tok->len;
      member_var->type = new_type(TY_INT);
      member_var->offset = offset;
      member_var->ext = FALSE;
      member_var->is_static = FALSE;
      offset++;
      member_var->next = enum_members;
      enum_members = member_var;
      if (consume(",")) {
        continue;
      } else if (consume("}")) {
        break;
      } else {
        error_at(token->str, "expected ',' after member declaration [in typedef]");
      }
    }
    Token *tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
    } else if (find_enum(tok)) {
      error_at(tok->str, "duplicated enum name: %.*s [in typedef]", tok->len, tok->str);
    }
    Enum *enum_ = malloc(sizeof(Enum));
    enum_->name = tok->str;
    enum_->len = tok->len;
    enum_->next = enums;
    enums = enum_;
  } else {
    error_at(token->str, "expected a struct but got \"%.*s\" [in typedef]", token->len, token->str);
  }
  expect(";", "after line", "typedef");
  node->endline = TRUE;
  return node;
}

Node *if_stmt() {
  free_token();
  Node *node = new_node(ND_IF);
  node->id = loop_cnt++;
  expect("(", "before condition", "if");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "if");
  node->then = stmt();
  if (token->kind == TK_ELSE) {
    free_token();
    node->els = stmt();
  } else {
    node->els = NULL;
  }
  return node;
}

Node *while_stmt() {
  free_token();
  expect("(", "before condition", "while");
  Node *node = new_node(ND_WHILE);
  node->id = loop_cnt++;
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "while");
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  return node;
}

Node *do_while_stmt() {
  free_token();
  Node *node = new_node(ND_DOWHILE);
  node->id = loop_cnt++;
  node->then = stmt();
  if (token->kind != TK_WHILE) {
    error_at(token->str, "expected 'while' but got \"%.*s\" [in do-while statement]", token->len, token->str);
  }
  free_token();
  expect("(", "before condition", "do-while");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "do-while");
  int loop_id_prev = loop_id;
  loop_id = node->id;
  loop_id = loop_id_prev;
  expect(";", "after line", "do-while");
  node->endline = TRUE;
  return node;
}

Node *for_stmt() {
  int init;
  free_token();
  expect("(", "before initialization", "for");
  Node *node = new_node(ND_FOR);
  node->id = loop_cnt++;
  if (consume(";")) {
    node->init = new_node(ND_NONE);
    node->init->endline = TRUE;
    init = FALSE;
  } else if (is_type(token)) {
    Type *type = consume_type();
    Token *tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
    }
    node->init = local_variable_declaration(tok, type, FALSE);
    expect(";", "after initialization", "for");
    node->init->endline = TRUE;
    init = TRUE;
  } else {
    node->init = expr();
    expect(";", "after initialization", "for");
    node->init->endline = TRUE;
    init = FALSE;
  }
  if (consume(";")) {
    node->cond = new_num(1);
    node->cond->endline = FALSE;
  } else {
    node->cond = expr();
    node->cond->endline = FALSE;
    expect(";", "after condition", "for");
  }
  if (consume(")")) {
    node->step = new_node(ND_NONE);
    node->step->endline = TRUE;
  } else {
    node->step = expr();
    node->step->endline = TRUE;
    expect(")", "after step expression", "for");
  }
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  if (init) {
    current_fn->locals = current_fn->locals->next;
  }
  loop_id = loop_id_prev;
  return node;
}

Node *break_stmt() {
  if (loop_id == -1) {
    error_at(token->str, "stray break statement [in break statement]");
  }
  free_token();
  expect(";", "after line", "break");
  Node *node = new_node(ND_BREAK);
  node->endline = TRUE;
  node->id = loop_id;
  return node;
}

Node *continue_stmt() {
  if (loop_id == -1) {
    error_at(token->str, "stray continue statement [in continue statement]");
  }
  free_token();
  expect(";", "after line", "continue");
  Node *node = new_node(ND_CONTINUE);
  node->endline = TRUE;
  node->id = loop_id;
  return node;
}

Node *return_stmt() {
  free_token();
  Node *node = new_node(ND_RETURN);
  if (consume(";")) {
    node->rhs = new_num(0);
  } else {
    node->rhs = expr();
    expect(";", "after line", "return");
  }
  node->endline = TRUE;
  return node;
}

Node *stmt() {
  Node *node;
  if (consume("{")) {
    node = block_stmt();
    expect("}", "after block", "block");
  } else if (token->kind == TK_EXTERN) {
    node = extern_stmt();
    expect(";", "after line", "extern declaration");
  } else if (token->kind == TK_GOTO) {
    node = goto_stmt();
    expect(";", "after line", "goto statement");
  } else if (token->kind == TK_LABEL) {
    node = label_stmt();
  } else if (token->kind == TK_STATIC) {
    free_token();
    node = vardec_and_funcdef_stmt(TRUE);
  } else if (is_type(token)) {
    node = vardec_and_funcdef_stmt(FALSE);
  } else if (token->kind == TK_STRUCT) {
    node = struct_stmt();
  } else if (token->kind == TK_TYPEDEF) {
    node = typedef_stmt();
  } else if (token->kind == TK_IF) {
    node = if_stmt();
  } else if (token->kind == TK_WHILE) {
    node = while_stmt();
  } else if (token->kind == TK_DO) {
    node = do_while_stmt();
  } else if (token->kind == TK_FOR) {
    node = for_stmt();
  } else if (token->kind == TK_BREAK) {
    node = break_stmt();
  } else if (token->kind == TK_CONTINUE) {
    node = continue_stmt();
  } else if (token->kind == TK_RETURN) {
    node = return_stmt();
  } else {
    node = expr();
    expect(";", "after line", "expression");
    node->endline = TRUE;
  }
  return node;
}

void program() {
  int i = 0;
  code = NULL;
  while (token->kind != TK_EOF) {
    code = realloc(code, sizeof(Node *) * ++i);
    code[i - 1] = stmt();
    if (!code)
      error("realloc failed");
  }
  code[i] = new_node(ND_NONE);
}

Node *expr() { return assign(); }

Node *assign_sub(Node *lhs, Node *rhs, char *ptr) {
  if (lhs->type->const_) {
    error_at(ptr, "constant variable cannot be assigned [in assign_sub]");
  }
  Node *node = new_binary(ND_ASSIGN, lhs, rhs);
  return node;
}

Node *assign() {
  Node *node = logical_or();
  char *ptr;
  if (consume("=")) {
    ptr = consumed_ptr;
    node = assign_sub(node, expr(), ptr);
  } else if (consume("+=")) {
    node = assign_sub(node, new_binary(ND_ADD, node, expr()), ptr);
  } else if (consume("-=")) {
    node = assign_sub(node, new_binary(ND_SUB, node, expr()), ptr);
  } else if (consume("*=")) {
    node = assign_sub(node, new_binary(ND_MUL, node, expr()), ptr);
  } else if (consume("/=")) {
    node = assign_sub(node, new_binary(ND_DIV, node, expr()), ptr);
  } else if (consume("%=")) {
    node = assign_sub(node, new_binary(ND_MOD, node, expr()), ptr);
  }
  return node;
}

Node *logical_or() {
  Node *node = logical_and();
  for (;;) {
    if (consume("||")) {
      node = new_binary(ND_OR, node, logical_and());
      node->type = node->lhs->type;
      node->id = logical_cnt++;
    } else {
      break;
    }
  }
  return node;
}

Node *logical_and() {
  Node *node = bit_or();
  for (;;) {
    if (consume("&&")) {
      node = new_binary(ND_AND, node, bit_or());
      node->type = node->lhs->type;
      node->id = logical_cnt++;
    } else {
      break;
    }
  }
  return node;
}

Node *bit_or() {
  Node *node = bit_xor();
  for (;;) {
    if (consume("|")) {
      node = new_binary(ND_BITOR, node, bit_xor());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

Node *bit_xor() {
  Node *node = bit_and();
  for (;;) {
    if (consume("^")) {
      node = new_binary(ND_BITXOR, node, bit_and());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

Node *bit_and() {
  Node *node = equality();
  for (;;) {
    if (consume("&")) {
      node = new_binary(ND_BITAND, node, equality());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_binary(ND_EQ, node, relational());
      node->type = new_type(TY_INT);
    } else if (consume("!=")) {
      node = new_binary(ND_NE, node, relational());
      node->type = new_type(TY_INT);
    } else {
      break;
    }
  }
  return node;
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = bit_shift();

  for (;;) {
    if (consume("<")) {
      node = new_binary(ND_LT, node, bit_shift());
      node->type = new_type(TY_INT);
    } else if (consume("<=")) {
      node = new_binary(ND_LE, node, bit_shift());
      node->type = new_type(TY_INT);
    } else if (consume(">")) {
      node = new_binary(ND_LT, bit_shift(), node);
      node->type = new_type(TY_INT);
    } else if (consume(">=")) {
      node = new_binary(ND_LE, bit_shift(), node);
      node->type = new_type(TY_INT);
    } else {
      break;
    }
  }
  return node;
}

Node *bit_shift() {
  Node *node = add();
  for (;;) {
    if (consume("<<")) {
      node = new_binary(ND_SHL, node, add());
      node->type = node->lhs->type;
    } else if (consume(">>")) {
      node = new_binary(ND_SHR, node, add());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

// ポインタ + 整数 または 整数 + ポインタ の場合に
// 整数側を型サイズで乗算するラッパ
Node *new_add(Node *lhs, Node *rhs, char *ptr) {
  Node *node;
  Node *mul_node;
  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(ptr, "invalid operands to binary expression [in new_add]");
  }
  // lhsがptr, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)));
    node = new_binary(ND_ADD, lhs, mul_node);
    node->type = lhs->type;
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    mul_node = new_binary(ND_MUL, lhs, new_num(get_sizeof(rhs->type->ptr_to)));
    node = new_binary(ND_ADD, mul_node, rhs);
    node->type = rhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_ADD, lhs, rhs);
    node->type = new_type(TY_INT);
  }
  return node;
}

Node *new_sub(Node *lhs, Node *rhs, char *ptr) {
  Node *node;
  Node *mul_node;

  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = lhs->type;
    node = new_binary(ND_DIV, node, new_num(get_sizeof(lhs->type->ptr_to)));
    node->type = new_type(TY_INT);
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(ptr, "invalid operands to binary expression [in new_sub]");
  }
  // lhsがptr, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)));
    node = new_binary(ND_SUB, lhs, mul_node);
    node->type = lhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = new_type(TY_INT);
  }
  return node;
}

// add = mul ("+" mul | "-" mul)*
// ポインタ演算を挟み込む
Node *add() {
  char *consumed_ptr_prev;
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_add(node, mul(), consumed_ptr_prev);
    } else if (consume("-")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_sub(node, mul(), consumed_ptr_prev);
    } else {
      break;
    }
  }
  return node;
}

Type *resolve_type_mul(Type *left, Type *right, char *ptr) {
  if (is_number(left) && is_number(right)) {
    return new_type(TY_INT);
  }
  error_at(ptr, "invalid operands to binary expression [in resolve_type_mul]");
  return NULL;
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  char *consumed_ptr_prev;
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_MUL, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else if (consume("/")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_DIV, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else if (consume("%")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_MOD, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else {
      break;
    }
  }
  return node;
}

// unary = ("+" | "-")? unary
//       | primary
Node *unary() {
  Node *node;
  if (token->kind == TK_SIZEOF) {
    free_token();
    return new_num(get_sizeof(unary()->type));
  }
  if (consume("+"))
    return increment_decrement();
  if (consume("-")) {
    node = new_binary(ND_SUB, new_num(0), increment_decrement());
    node->type = node->rhs->type;
    return node;
  }
  if (consume("&")) {
    node = new_node(ND_ADDR);
    node->lhs = unary();
    node->type = new_type_ptr(node->lhs->type);
    return node;
  }
  if (consume("*")) {
    char *consumed_ptr_prev = consumed_ptr;
    node = unary();
    node = new_deref(node);
    if (!is_ptr_or_arr(node->lhs->type)) {
      error_at(consumed_ptr_prev, "invalid pointer dereference");
    }
    return node;
  }
  if (consume("!")) {
    node = new_node(ND_NOT);
    node->lhs = unary();
    node->type = node->lhs->type;
    return node;
  }
  if (consume("~")) {
    node = new_node(ND_BITNOT);
    node->lhs = unary();
    node->type = node->lhs->type;
    return node;
  }
  return increment_decrement();
}

Node *increment_decrement() {
  Node *node;
  char *ptr;
  if (consume("++")) {
    ptr = consumed_ptr;
    node = access_member();
    return assign_sub(node, new_add(node, new_num(1), consumed_ptr), ptr);
  } else if (consume("--")) {
    ptr = consumed_ptr;
    node = access_member();
    return assign_sub(node, new_sub(node, new_num(1), consumed_ptr), ptr);
  }
  node = access_member();
  if (consume("++")) {
    node = new_binary(ND_POSTINC, node, new_add(node, new_num(1), consumed_ptr));
  } else if (consume("--")) {
    node = new_binary(ND_POSTINC, node, new_sub(node, new_num(1), consumed_ptr));
  }
  return node;
}

// Structure Reference and Array Indexing
Node *access_member() {
  Node *ptr;
  Node *offset_node;
  Token *tok;
  Struct *struct_;
  LVar *var;
  char *consumed_ptr_prev;
  Token *prev_tok = token;
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      consumed_ptr_prev = consumed_ptr;
      if (!is_ptr_or_arr(node->type)) {
        error_at(consumed_ptr_prev, "invalid array access [in primary]");
      }
      node = new_add(node, expr(), consumed_ptr_prev);
      expect("]", "after number", "array access");
      node = new_deref(node);
    } else if (consume(".")) {
      if (node->type->ty != TY_STRUCT) {
        error_at(prev_tok->str, "%.*s is not a struct [in struct reference]", prev_tok->len, prev_tok->str);
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in struct reference]", token->len, token->str);
      }
      struct_ = node->type->struct_;
      if (!struct_) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!struct_->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(struct_, tok);
      offset_node = new_num(var->offset);
      ptr = new_node(ND_ADDR);
      ptr->lhs = node;
      ptr->type = new_type_ptr(node->type);
      ptr = new_binary(ND_ADD, ptr, offset_node);
      ptr->type = new_type_ptr(var->type);
      node = new_deref(ptr);
    } else if (consume("->")) {
      if (node->type->ty != TY_PTR) {
        error_at(prev_tok->str, "%.*s is not a pointer [in struct reference]", prev_tok->len, prev_tok->str);
      }
      if (node->type->ptr_to->ty != TY_STRUCT) {
        error_at(prev_tok->str, "%.*s is not a pointer of a struct [in struct reference]", prev_tok->len,
                 prev_tok->str);
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in struct reference]", token->len, token->str);
      }
      struct_ = node->type->ptr_to->struct_;
      if (!struct_) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!struct_->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(struct_, tok);
      offset_node = new_num(var->offset);
      ptr = new_binary(ND_ADD, node, offset_node);
      ptr->type = new_type_ptr(var->type);
      node = new_deref(ptr);
    } else
      break;
  }
  return node;
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  Token *tok;
  Type *type;

  // 括弧
  if (consume("(")) {
    node = expr();
    expect(")", "after expression", "primary");
    return node;
  }

  // 数値
  if (token->kind == TK_NUM) {
    return new_num(expect_number());
  }

  // 文字列
  if (token->kind == TK_STRING) {
    String *str = malloc(sizeof(String));
    str->text = token->str;
    str->len = token->len;
    str->id = label_cnt++;
    str->next = strings;
    strings = str;
    free_token();
    node = new_node(ND_STRING);
    node->id = str->id;
    node->type = new_type_ptr(new_type(TY_CHAR));
    return node;
  }

  // 型
  type = consume_type();
  if (type) {
    node = new_node(ND_TYPE);
    node->type = type;
    return node;
  }

  tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in primary]", token->len, token->str);
    return NULL;
  }

  // enumのメンバー
  LVar *member = find_enum_member(tok);
  if (member) {
    node = new_num(member->offset);
    return node;
  }

  // 変数
  if (!consume("(")) {
    LVar *lvar = find_lvar(tok);
    LVar *gvar = find_gver(tok);
    if (lvar) {
      node = new_node(ND_LVAR);
      node->var = lvar;
      node->type = lvar->type;
    } else if (gvar) {
      node = new_node(ND_GVAR);
      node->var = gvar;
      node->type = gvar->type;

    } else {
      error_at(tok->str, "undefined variable: %.*s [in primary]", tok->len, tok->str);
    }
    return node;
  }

  // 関数呼び出し
  else {
    Function *fn = find_fn(tok);
    if (!fn) {
      error_at(tok->str, "undefined function: %.*s [in primary]", tok->len, tok->str);
    }
    node = new_node(ND_FUNCALL);
    node->fn = fn;
    node->id = loop_cnt++;
    node->type = fn->type;
    if (!consume(")")) {
      int n = 0;
      for (int i = 0; i < 6; i++) {
        node->args[i] = expr();
        n += 1;
        if (!consume(","))
          break;
      }
      node->val = n;
      expect(")", "after arguments", "function call");
    } else {
      node->val = 0;
    }
    return node;
  }
}
