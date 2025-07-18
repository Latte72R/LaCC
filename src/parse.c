
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

void error_duplicate_name(Token *tok, const char *type) {
  error_at(tok->str, "duplicated %s name: %.*s", type, tok->len, tok->str);
}

void *safe_realloc_array(void *ptr, int element_size, int new_size) {
  void *new_ptr = realloc(ptr, element_size * new_size);
  if (!new_ptr)
    error("realloc failed");
  return new_ptr;
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
  token = token->next;
  return TRUE;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  consumed_ptr = token->str;
  token = token->next;
  return tok;
}

Token *consume_ident_safe(const char *context) {
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in %s]", token->len, token->str, context);
  }
  return tok;
}

Type *parse_base_type_internal(int should_consume) {
  Token *tok = token;
  Type *type = malloc(sizeof(Type));

  // const修飾子の処理
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    token = token->next;
  } else {
    type->const_ = FALSE;
  }

  // 型の判定
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

  // 後続のconst
  if (token->kind == TK_CONST) {
    type->const_ = TRUE;
    token = token->next;
  }

  if (!should_consume)
    token = tok;
  return type;
}

Type *check_base_type() { return parse_base_type_internal(FALSE); }

Type *consume_type() {
  Type *type = parse_base_type_internal(TRUE);
  if (!type)
    return NULL;

  // ポインタ修飾子の処理
  while (consume("*")) {
    Type *ptr = malloc(sizeof(Type));
    ptr->ty = TY_PTR;
    ptr->ptr_to = type;
    type = ptr;
    if (token->kind == TK_CONST) {
      type->const_ = TRUE;
      token = token->next;
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
  token = token->next;
}

// Ensure that the current token is TK_NUM.
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number but got \"%.*s\" [in expect_number]", token->len, token->str);
  int val = token->val;
  token = token->next;
  return val;
}

int parse_sign() {
  if (consume("-"))
    return -1;
  if (consume("+"))
    return 1;
  return 1;
}

// 数値の期待値取得（符号付き）
int expect_signed_number() {
  int sign = parse_sign();
  return expect_number() * sign;
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

// 共通の変数作成処理
static LVar *new_lvar(Token *tok, Type *type, int is_static, int is_extern) {
  LVar *lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  lvar->ext = is_extern;
  lvar->is_static = is_static;
  lvar->type = type;
  return lvar;
}

// 配列サイズ解析の共通処理
Type *parse_array_dimensions(Type *base_type) {
  Type *type = base_type;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "array declaration");
  }
  return type;
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
    Token *tok_lvar = consume_ident_safe("function definition");
    type = parse_array_dimensions(type);
    if (type->ty == TY_ARR) {
      type->ty = TY_ARGARR;
    }
    Node *nd_lvar = new_node(ND_LVAR);
    node->args[i] = nd_lvar;
    LVar *lvar = new_lvar(tok_lvar, type, FALSE, FALSE);
    lvar->next = fn->locals;
    if (is_ptr_or_arr(type)) {
      lvar->offset = fn->locals->offset + 8;
    } else {
      lvar->offset = fn->locals->offset + get_sizeof(type);
    }
    fn->offset = lvar->offset;
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
    error_duplicate_name(tok, "local variable declaration");
  }
  Node *node = new_node(ND_VARDEC);
  lvar = new_lvar(tok, type, is_static, FALSE);
  Type *org_type = type;
  type = parse_array_dimensions(type);
  if (is_static) {
    lvar->block = block_cnt;
    LVar *static_lvar = new_lvar(tok, type, TRUE, FALSE);
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
      lvar->offset = expect_signed_number();
    } else {
      lvar->offset = 0;
    }
  } else {
    if (consume("=")) {
      if (consume("{")) {
        if (type->ty != TY_ARR) {
          error_at(token->str, "array initializer is only allowed for array type [in local variable declaration]");
        }
        Array *array = malloc(sizeof(Array));
        array->next = arrays;
        arrays = array;
        array->id = array_cnt++;
        array->byte = get_sizeof(org_type);
        array->val = NULL;
        int i = 0;
        do {
          array->val = safe_realloc_array(array->val, sizeof(int), i + 1);
          array->val[i++] = expect_number();
        } while (consume(","));
        array->len = i;
        expect("}", "after array initializer", "local variable declaration");
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
    error_duplicate_name(tok, "global variable declaration");
  }
  Node *node = new_node(ND_GLBDEC);
  type = parse_array_dimensions(type);
  lvar = new_lvar(tok, type, is_static, FALSE);
  lvar->next = globals;
  globals = lvar;
  node->var = lvar;
  node->type = type;

  // 要修正
  if (consume("=")) {
    lvar->offset = expect_signed_number();
  } else {
    lvar->offset = 0;
  }
  node->endline = TRUE;
  return node;
}

Node *vardec_and_funcdef_stmt(int is_static) {
  // 変数宣言または関数定義
  Type *ch_type = check_base_type();
  Type *type = consume_type();
  Token *tok = consume_ident_safe("variable declaration");

  // 関数定義
  if (consume("(")) {
    if (current_fn->next) {
      error_at(token->str, "nested function is not supported [in function definition]");
    } else {
      return function_definition(tok, type, is_static);
    }
  }

  Node *node = new_node(ND_BLOCK);
  node->body = malloc(sizeof(Node *));
  int i = 0;

  // 最初の変数
  if (current_fn->next) {
    node->body[i++] = local_variable_declaration(tok, type, is_static);
  } else {
    node->body[i++] = global_variable_declaration(tok, type, is_static);
  }

  // 追加の変数
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
    tok = consume_ident_safe("variable declaration");
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    if (current_fn->next) {
      node->body[i++] = local_variable_declaration(tok, type, is_static);
    } else {
      node->body[i++] = global_variable_declaration(tok, type, is_static);
    }
  }
  node->body[i] = new_node(ND_NONE);
  expect(";", "after line", "variable declaration");
  node->endline = TRUE;

  return node;
}

Node *extern_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gver(tok);
  Node *node = new_node(ND_EXTERN);
  if (lvar) {
    return node;
  }
  type = parse_array_dimensions(type);
  lvar = new_lvar(tok, type, FALSE, TRUE);
  lvar->next = globals;
  globals = lvar;
  node->var = lvar;
  node->type = type;
  node->endline = TRUE;
  return node;
}

Node *struct_stmt() {
  token = token->next;
  Token *tok = consume_ident_safe("struct declaration");
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
  while (!consume("}")) {
    Type *type = consume_type();
    Token *member_tok = consume_ident_safe("struct member declaration");
    Type *org_type = type;
    type = parse_array_dimensions(type);
    LVar *member_var = new_lvar(member_tok, type, FALSE, FALSE);
    member_var->next = struct_->var;
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
  while (!consume("}")) {
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    node->body[i++] = stmt();
  }
  node->body[i] = new_node(ND_NONE);
  current_fn->locals = var;
  block_cnt++;
  return node;
}

Node *extern_stmt() {
  token = token->next;
  Type *ch_type = check_base_type();
  Type *type = consume_type();
  Token *tok = consume_ident_safe("extern declaration");
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
    tok = consume_ident_safe("variable declaration");
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    node->body[i++] = extern_declaration(tok, type);
  }
  node->body[i] = new_node(ND_NONE);
  node->endline = TRUE;
  return node;
}

Node *goto_stmt() {
  token = token->next;
  Token *tok = consume_ident_safe("goto statement");
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
  token = token->next;
  node->endline = TRUE;
  return node;
}

Node *typedef_stmt() {
  token = token->next;
  Node *node;
  if (token->kind == TK_STRUCT) {
    token = token->next;
    node = new_node(ND_TYPEDEF);
    Token *tok1 = consume_ident_safe("typedef");
    Token *tok2 = consume_ident_safe("typedef");
    if (find_struct_tag(tok1)) {
      error_duplicate_name(tok1, "typedef");
    }
    if (find_struct(tok2)) {
      error_duplicate_name(tok2, "typedef");
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
    token = token->next;
    node = new_node(ND_TYPEDEF);
    int offset = 0;
    expect("{", "before enum members", "enum");
    do {
      Token *member_tok = consume_ident_safe("typedef");
      LVar *member_var = new_lvar(member_tok, new_type(TY_INT), FALSE, FALSE);
      member_var->offset = offset++;
      member_var->next = enum_members;
      enum_members = member_var;
    } while (consume(","));
    expect("}", "after enum members", "enum");
    Token *tok = consume_ident_safe("typedef");
    if (find_enum(tok)) {
      error_duplicate_name(tok, "typedef");
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
  token = token->next;
  Node *node = new_node(ND_IF);
  node->id = loop_cnt++;
  expect("(", "before condition", "if");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "if");
  node->then = stmt();
  if (token->kind == TK_ELSE) {
    token = token->next;
    node->els = stmt();
  } else {
    node->els = NULL;
  }
  return node;
}

Node *while_stmt() {
  token = token->next;
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
  token = token->next;
  Node *node = new_node(ND_DOWHILE);
  node->id = loop_cnt++;
  node->then = stmt();
  if (token->kind != TK_WHILE) {
    error_at(token->str, "expected 'while' but got \"%.*s\" [in do-while statement]", token->len, token->str);
  }
  token = token->next;
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
  token = token->next;
  expect("(", "before initialization", "for");
  Node *node = new_node(ND_FOR);
  node->id = loop_cnt++;
  if (consume(";")) {
    node->init = new_node(ND_NONE);
    node->init->endline = TRUE;
    init = FALSE;
  } else if (is_type(token)) {
    Type *type = consume_type();
    Token *tok = consume_ident_safe("variable declaration");
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
  token = token->next;
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
  token = token->next;
  expect(";", "after line", "continue");
  Node *node = new_node(ND_CONTINUE);
  node->endline = TRUE;
  node->id = loop_id;
  return node;
}

Node *return_stmt() {
  token = token->next;
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
  } else if (token->kind == TK_EXTERN) {
    node = extern_stmt();
    expect(";", "after line", "extern declaration");
  } else if (token->kind == TK_GOTO) {
    node = goto_stmt();
    expect(";", "after line", "goto statement");
  } else if (token->kind == TK_LABEL) {
    node = label_stmt();
  } else if (token->kind == TK_STATIC) {
    token = token->next;
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
    code = safe_realloc_array(code, sizeof(Node *), i + 1);
    code[i++] = stmt();
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
    token = token->next;
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
      tok = consume_ident_safe("struct reference");
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
      tok = consume_ident_safe("struct reference");
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
    token = token->next;
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

  tok = consume_ident_safe("primary");

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
