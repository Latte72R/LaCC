
#include "lcc.h"

//
// Parser
//

extern Node *code[100];
extern Token *token;
extern Function *functions;
extern Function *current_fn;
extern int labelseq;
extern int loop_id;
extern LVar *globals;
extern Struct *structs;
extern StructTag *struct_tags;
extern Enum *enums;
extern LVar *enum_members;
extern String *strings;

char *consumed_ptr;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = current_fn->locals; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_gver(Token *tok) {
  for (LVar *var = globals; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structを名前で検索する。見つからなかった場合はNULLを返す。
Struct *find_struct(Token *tok) {
  for (Struct *var = structs; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumを名前で検索する。見つからなかった場合はNULLを返す。
Enum *find_enum(Token *tok) {
  for (Enum *var = enums; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_enum_member(Token *tok) {
  for (LVar *var = enum_members; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_struct_member(Struct *struct_, Token *tok) {
  for (LVar *var = struct_->var; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// struct_tagを名前で検索する。見つからなかった場合はNULLを返す。
StructTag *find_struct_tag(Token *tok) {
  for (StructTag *var = struct_tags; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 関数を名前で検索する。見つからなかった場合はNULLを返す。
Function *find_fn(Token *tok) {
  for (Function *fn = functions; fn; fn = fn->next)
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

Type *consume_type() {
  Token *tok = token;
  Type *type = calloc(1, sizeof(Type));
  if (tok->kind == TK_IDENT) {
    Struct *struct_ = find_struct(tok);
    Enum *enum_ = find_enum(tok);
    if (struct_) {
      type->ty = TY_STRUCT;
      type->struct_ = struct_;
    } else if (enum_) {
      type->ty = TY_INT;
    } else {
      return NULL;
    }
  } else if (tok->kind != TK_TYPE) {
    return NULL;
  } else if (memcmp(tok->str, "int", tok->len) == 0) {
    type->ty = TY_INT;
  } else if (memcmp(tok->str, "char", tok->len) == 0) {
    type->ty = TY_CHAR;
  } else if (memcmp(tok->str, "void", tok->len) == 0) {
    type->ty = TY_VOID;
  } else {
    error_at(tok->str, "unknown type \"%.*s\" [in consume type]", tok->len, tok->str);
  }
  consumed_ptr = token->str;
  token = token->next;
  while (consume("*")) {
    Type *ptr = calloc(1, sizeof(Type));
    ptr->ty = TY_PTR;
    ptr->ptr_to = type;
    type = ptr;
  }
  return type;
}

int is_type(Token *tok) {
  if (tok->kind == TK_TYPE)
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

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR; }
int is_number(Type *type) { return type->ty == TY_INT || type->ty == TY_CHAR; }

// 変数として扱うときのサイズ
int get_type_size(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_CHAR) {
    return 1;
  } else if (is_ptr_or_arr(type)) {
    return 8;
  } else {
    error_at(token->str, "invalid type (%d) [in get_type_size]", type->ty);
    return 0;
  }
}

// 予約しているスタック領域のサイズ
int get_sizeof(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_CHAR) {
    return 1;
  } else if (type->ty == TY_PTR) {
    return 8;
  } else if (type->ty == TY_ARR) {
    return get_sizeof(type->ptr_to) * type->array_size;
  } else if (type->ty == TY_STRUCT) {
    return type->struct_->size;
  } else {
    error_at(token->str, "invalid type [in get_sizeof]");
    return 0;
  }
}

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->endline = FALSE;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  node->type = lhs->type;
  return node;
}

Type *new_type_int() {
  Type *type = calloc(1, sizeof(Type));
  type->ty = TY_INT;
  return type;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  node->type = new_type_int();
  return node;
}

Node *function_definition(Token *tok, Type *type) {
  Function *fn = find_fn(tok);
  if (fn) {
    // error_at(token->str, "duplicated function name: %.*s", tok->len, tok->str);
  } else {
    fn = calloc(1, sizeof(Function));
    fn->next = functions;
    functions = fn;
  }
  fn->name = tok->str;
  fn->len = tok->len;
  fn->locals = calloc(1, sizeof(LVar));
  fn->locals->offset = 0;
  fn->type = type;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->fn = fn;
  if (!consume(")")) {
    for (int i = 0; i < 4; i++) {
      type = consume_type();
      Token *tok_lvar = consume_ident();
      if (!tok_lvar) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in function definition]", token->len,
                 token->str);
      }
      Node *nd_lvar = new_node(ND_LVAR);
      node->args[i] = nd_lvar;
      LVar *lvar = calloc(1, sizeof(LVar));
      lvar->next = fn->locals;
      lvar->name = tok_lvar->str;
      lvar->len = tok_lvar->len;
      lvar->offset = fn->locals->offset + get_sizeof(type);
      lvar->type = type;
      nd_lvar->var = lvar;
      nd_lvar->type = type;
      fn->locals = lvar;
      if (!consume(","))
        break;
    }
    expect(")", "after arguments", "function definition");
  }
  if (!(token->kind == TK_RESERVED && !memcmp(token->str, "{", token->len))) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
  } else {
    node->lhs = stmt();
  }
  current_fn = prev_fn;
  return node;
}

Node *local_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_lvar(tok);
  if (lvar) {
    error_at(tok->str, "duplicated variable name: %.*s [in variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_VARDEC);
  lvar = calloc(1, sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  if (consume("[")) {
    Type *arr_type = calloc(1, sizeof(Type));
    arr_type->ty = TY_ARR;
    arr_type->ptr_to = type;
    arr_type->array_size = expect_number();
    type = arr_type;
    expect("]", "after number", "array declaration");
    lvar->offset = current_fn->locals->offset + get_sizeof(type);
  } else {
    type->array_size = 1;
    lvar->offset = current_fn->locals->offset + get_sizeof(type);
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = current_fn->locals;
  current_fn->locals = lvar;
  if (consume("=")) {
    node = new_binary(ND_ASSIGN, node, logical());
  }
  expect(";", "after line", "variable declaration");
  node->endline = TRUE;
  return node;
}

Node *global_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gver(tok);
  if (lvar) {
    error_at(token->str, "duplicated variable name: %.*s [in global variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_GLBDEC);
  lvar = calloc(1, sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  if (consume("[")) {
    Type *arr_type = calloc(1, sizeof(Type));
    arr_type->ty = TY_ARR;
    arr_type->ptr_to = type;
    arr_type->array_size = expect_number();
    type = arr_type;
    expect("]", "after number", "array declaration");
  } else {
    type->array_size = 1;
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = globals;
  globals = lvar;
  if (consume("=")) {
    int sign = 1;
    if (consume("-")) {
      sign = -1;
    } else if (consume("+")) {
      sign = 1;
    }
    lvar->offset = expect_number() * sign;
  }
  expect(";", "after line", "global variable declaration");
  node->endline = TRUE;
  return node;
}

Node *new_struct(Token *tok) {
  Node *node = new_node(ND_STRUCT);
  StructTag *struct_tag = find_struct_tag(tok);
  if (!struct_tag) {
    error_at(tok->str, "unknown tag: %.*s [in struct declaration]", tok->len, tok->str);
  }
  Struct *struct_ = struct_tag->main;
  int offset = 0;
  expect("{", "before struct members", "struct");
  while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
    Type *type = consume_type();
    Token *member_tok = consume_ident();
    if (!member_tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in struct declaration]", token->len, token->str);
    }
    if (consume("[")) {
      Type *arr_type = calloc(1, sizeof(Type));
      arr_type->ty = TY_ARR;
      arr_type->ptr_to = type;
      arr_type->array_size = expect_number();
      type = arr_type;
      expect("]", "after number", "struct");
    } else {
      type->array_size = 1;
    }
    LVar *member_var = calloc(1, sizeof(LVar));
    member_var->name = member_tok->str;
    member_var->len = member_tok->len;
    member_var->type = type;
    member_var->next = struct_->var;
    struct_->var = member_var;
    int type_size = get_type_size(type);
    if (offset % type_size != 0) {
      offset += type_size - (offset % type_size);
    }
    member_var->offset = offset;
    offset += type_size;
    if (consume(";")) {
      continue;
    } else {
      error_at(token->str, "expected ';' after struct member declaration [in struct declaration]");
    }
  }
  struct_->size = offset;
  expect("}", "after struct members", "struct");
  expect(";", "after struct definition", "struct");
  node->endline = TRUE;
  return node;
}

Node *stmt() {
  Node *node;
  Token *tok;
  Type *type;
  int loop_id_prev;
  if (consume("{")) {
    node = new_node(ND_BLOCK);
    node->body = calloc(1, sizeof(Node *));
    int i = 0;
    while (!(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
      node->body[i++] = stmt();
      node->body = realloc(node->body, sizeof(Node *) * (i + 1));
    }
    node->body[i] = new_node(ND_NONE);
    expect("}", "after block", "block");
  } else if (is_type(token)) {
    // 変数宣言または関数定義
    type = consume_type();
    tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
    }
    if (consume("(")) {
      // 関数定義
      if (current_fn) {
        error_at(token->str, "nested function is not supported [in function definition]");
      }
      node = function_definition(tok, type);
    } else if (current_fn) {
      // ローカル変数宣言
      node = local_variable_declaration(tok, type);
    } else {
      // グローバル変数宣言
      node = global_variable_declaration(tok, type);
    }
  } else if (token->kind == TK_STRUCT) {
    token = token->next;
    tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in struct definition]", token->len, token->str);
    }
    node = new_struct(tok);
  } else if (token->kind == TK_TYPEDEF) {
    token = token->next;
    if (token->kind == TK_STRUCT) {
      token = token->next;
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
      Struct *var = calloc(1, sizeof(Struct));
      var->name = tok2->str;
      var->len = tok2->len;
      var->next = structs;
      structs = var;
      StructTag *tag = calloc(1, sizeof(StructTag));
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
      while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
        Token *member_tok = consume_ident();
        if (!member_tok) {
          error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
        }
        LVar *member_var = calloc(1, sizeof(LVar));
        member_var->name = member_tok->str;
        member_var->len = member_tok->len;
        member_var->type = calloc(1, sizeof(Type));
        member_var->type->ty = TY_INT;
        member_var->offset = offset;
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
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
      } else if (find_enum(tok)) {
        error_at(tok->str, "duplicated enum name: %.*s [in typedef]", tok->len, tok->str);
      }
      expect(";", "after enum definition", "typedef");
      Enum *enum_ = calloc(1, sizeof(Enum));
      enum_->name = tok->str;
      enum_->len = tok->len;
      enum_->next = enums;
      enums = enum_;
      node->endline = TRUE;
      return node;
    } else {
      error_at(token->str, "expected a struct but got \"%.*s\" [in typedef]", token->len, token->str);
    }
    node->endline = TRUE;
    expect(";", "after line", "typedef");
  } else if (token->kind == TK_IF) {
    token = token->next;
    node = new_node(ND_IF);
    node->id = labelseq++;
    expect("(", "before condition", "if");
    node->cond = logical();
    node->cond->endline = TRUE;
    expect(")", "after equality", "if");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    } else {
      node->els = NULL;
    }
  } else if (token->kind == TK_WHILE) {
    token = token->next;
    expect("(", "before condition", "while");
    node = new_node(ND_WHILE);
    node->id = labelseq++;
    node->cond = logical();
    node->cond->endline = TRUE;
    expect(")", "after equality", "while");
    loop_id_prev = loop_id;
    loop_id = node->id;
    node->then = stmt();
    loop_id = loop_id_prev;
  } else if (token->kind == TK_FOR) {
    token = token->next;
    expect("(", "before initialization", "for");
    node = new_node(ND_FOR);
    node->id = labelseq++;
    if (is_type(token)) {
      type = consume_type();
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in for]", token->len, token->str);
      }
      node->init = local_variable_declaration(tok, type);
    } else {
      node->init = expr();
      node->init->endline = TRUE;
      expect(";", "after initialization", "for");
    }
    node->cond = logical();
    node->cond->endline = TRUE;
    expect(";", "after condition", "for");
    node->inc = expr();
    node->inc->endline = TRUE;
    expect(")", "after step expression", "for");
    loop_id_prev = loop_id;
    loop_id = node->id;
    node->then = stmt();
    loop_id = loop_id_prev;
  } else if (token->kind == TK_BREAK) {
    if (loop_id == -1) {
      error_at(token->str, "stray break statement [in break statement]");
    }
    token = token->next;
    expect(";", "after line", "break");
    node = new_node(ND_BREAK);
    node->endline = TRUE;
    node->id = loop_id;
  } else if (token->kind == TK_CONTINUE) {
    if (loop_id == -1) {
      error_at(token->str, "stray continue statement [in continue statement]");
    }
    token = token->next;
    expect(";", "after line", "continue");
    node = new_node(ND_CONTINUE);
    node->endline = TRUE;
    node->id = loop_id;
  } else if (token->kind == TK_RETURN) {
    token = token->next;
    node = new_node(ND_RETURN);
    node->rhs = logical();
    expect(";", "after line", "return");
    node->endline = TRUE;
  } else {
    node = expr();
    expect(";", "after line", "expression");
    node->endline = TRUE;
  }
  return node;
}

void program() {
  int i = 0;
  while (token->kind != TK_EOF)
    code[i++] = stmt();
  code[i] = NULL;
}

Node *expr() { return assign(); }

Node *assign() {
  Node *node = logical();
  if (consume("=")) {
    node = new_binary(ND_ASSIGN, node, logical());
  } else if (consume("+=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_ADD, node, logical()));
  } else if (consume("-=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_SUB, node, logical()));
  } else if (consume("*=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_MUL, node, logical()));
  } else if (consume("/=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_DIV, node, logical()));
  } else if (consume("%=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_MOD, node, logical()));
  }
  return node;
}

// logical = equality ("&&" equality | "||" equality)*
Node *logical() {
  Node *node = equality();
  while (TRUE) {
    if (consume("&&")) {
      node = new_binary(ND_AND, node, equality());
    } else if (consume("||")) {
      node = new_binary(ND_OR, node, equality());
    } else {
      return node;
    }
  }
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  while (TRUE) {
    if (consume("==")) {
      node = new_binary(ND_EQ, node, relational());
      node->type = new_type_int();
    } else if (consume("!=")) {
      node = new_binary(ND_NE, node, relational());
      node->type = new_type_int();
    } else {
      return node;
    }
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  while (TRUE) {
    if (consume("<")) {
      node = new_binary(ND_LT, node, add());
      node->type = new_type_int();
    } else if (consume("<=")) {
      node = new_binary(ND_LE, node, add());
      node->type = new_type_int();
    } else if (consume(">")) {
      node = new_binary(ND_LT, add(), node);
      node->type = new_type_int();
    } else if (consume(">=")) {
      node = new_binary(ND_LE, add(), node);
      node->type = new_type_int();
    } else {
      return node;
    }
  }
}

// ポインタ + 整数 または 整数 + ポインタ の場合に
// 整数側を型サイズで乗算するラッパ
Node *new_add(Node *lhs, Node *rhs, char *ptr) {
  Node *node;
  Node *mul_node;
  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = lhs->type;
  }
  // lhsがptr, rhsがintなら
  if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_type_size(lhs->type->ptr_to)));
    node = new_binary(ND_ADD, lhs, mul_node);
    node->type = lhs->type;
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    mul_node = new_binary(ND_MUL, lhs, new_num(get_type_size(rhs->type->ptr_to)));
    node = new_binary(ND_ADD, mul_node, rhs);
    node->type = rhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_ADD, lhs, rhs);
    node->type = new_type_int();
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
  }
  // lhsがint, rhsがptrなら
  if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(ptr, "invalid operands to binary expression [in new_sub]");
  }
  // lhsがptr, rhsがintなら
  if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_type_size(lhs->type->ptr_to)));
    node = new_binary(ND_SUB, lhs, mul_node);
    node->type = lhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = new_type_int();
  }
  return node;
}

// add = mul ("+" mul | "-" mul)*
// ポインタ演算を挟み込む
Node *add() {
  char *consumed_ptr_prev;
  Node *node = mul();
  while (TRUE) {
    if (consume("+")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_add(node, mul(), consumed_ptr_prev);
    } else if (consume("-")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_sub(node, mul(), consumed_ptr_prev);
    } else {
      return node;
    }
  }
}

Type *resolve_type_mul(Type *left, Type *right, char *ptr) {
  if (is_number(left) && is_number(right)) {
    return new_type_int();
  }
  error_at(ptr, "invalid operands to binary expression [in resolve_type_mul]");
  return NULL;
}
// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  char *consumed_ptr_prev;
  Node *node = unary();

  while (TRUE) {
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
      return node;
    }
  }
}

// unary = ("+" | "-")? unary
//       | primary
Node *unary() {
  Node *node;
  if (consume("sizeof")) {
    return new_num(get_sizeof(unary()->type));
  }
  if (consume("+"))
    return refer_struct();
  if (consume("-")) {
    node = new_binary(ND_SUB, new_num(0), refer_struct());
    node->type = node->rhs->type;
    return node;
  }
  if (consume("&")) {
    node = new_node(ND_ADDR);
    node->lhs = unary();
    node->type = calloc(1, sizeof(Type));
    node->type->ty = TY_PTR;
    node->type->ptr_to = node->lhs->type;
    return node;
  }
  if (consume("*")) {
    char *consumed_ptr_prev = consumed_ptr;
    node = new_node(ND_DEREF);
    node->lhs = unary();
    if (!is_ptr_or_arr(node->lhs->type)) {
      error_at(consumed_ptr_prev, "invalid pointer dereference");
    }
    node->type = node->lhs->type->ptr_to;
    return node;
  }
  if (consume("!")) {
    node = new_node(ND_NOT);
    node->lhs = unary();
    node->type = new_type_int();
    return node;
  }
  return refer_struct();
}

// Structure Reference
Node *refer_struct() {
  Token *prev_tok = token;
  Node *node = primary();
  while (TRUE) {
    if (consume("->")) {
      if (node->type->ty != TY_PTR) {
        error_at(prev_tok->str, "%.*s is not a pointer [in struct reference]", prev_tok->len, prev_tok->str);
      }
      Token *tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in struct reference]", token->len, token->str);
      }
      Struct *struct_ = node->type->ptr_to->struct_;
      if (!struct_) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!struct_->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      LVar *var = find_struct_member(struct_, tok);
      Node *offset_node = new_num(var->offset);
      Node *ptr = new_binary(ND_ADD, node, offset_node);
      Type *type = calloc(1, sizeof(Type));
      type->ty = TY_PTR;
      type->ptr_to = var->type;
      ptr->type = type;
      if (consume("[")) {
        char *consumed_ptr_prev = consumed_ptr;
        if (!is_ptr_or_arr(var->type)) {
          error_at(consumed_ptr_prev, "invalid array access [in struct reference]");
        }
        ptr = new_add(ptr, logical(), consumed_ptr_prev);
        expect("]", "after number", "struct reference");
        node = new_node(ND_DEREF);
        node->lhs = ptr;
        node->type = var->type->ptr_to;
      } else {
        node = new_node(ND_DEREF);
        node->lhs = ptr;
        node->type = var->type;
      }
    } else
      return node;
  }
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  Token *tok;
  Type *type;

  // 括弧
  if (consume("(")) {
    node = logical();
    expect(")", "after expression", "primary");
    if (consume("++")) {
      node = new_binary(ND_ASSIGN, node, new_add(node, new_num(1), consumed_ptr));
    } else if (consume("--")) {
      node = new_binary(ND_ASSIGN, node, new_sub(node, new_num(1), consumed_ptr));
    }
    return node;
  }

  // 数値
  if (token->kind == TK_NUM) {
    return new_num(expect_number());
  }

  // 文字列
  if (token->kind == TK_STR) {
    String *str = calloc(1, sizeof(String));
    str->text = token->str;
    str->len = token->len;
    str->label = labelseq++;
    str->next = strings;
    strings = str;
    token = token->next;
    node = new_node(ND_STR);
    node->str = str;
    node->type = calloc(1, sizeof(Type));
    node->type->ty = TY_PTR;
    type = calloc(1, sizeof(Type));
    type->ty = TY_CHAR;
    node->type->ptr_to = type;
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
    if (consume("[")) {
      char *consumed_ptr_prev = consumed_ptr;
      if (!is_ptr_or_arr(node->type)) {
        error_at(consumed_ptr_prev, "invalid array access [in primary]");
      }
      node = new_add(node, logical(), consumed_ptr_prev);
      expect("]", "after number", "array access");
      Node *nd_deref = new_node(ND_DEREF);
      nd_deref->lhs = node;
      node = nd_deref;
      node->type = node->lhs->type->ptr_to;
    }
    if (consume("++")) {
      node = new_binary(ND_ASSIGN, node, new_add(node, new_num(1), consumed_ptr));
    } else if (consume("--")) {
      node = new_binary(ND_ASSIGN, node, new_sub(node, new_num(1), consumed_ptr));
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
    node->id = labelseq++;
    node->type = fn->type;
    if (!consume(")")) {
      for (int i = 0; i < 6; i++) {
        node->args[i] = logical();
        if (!consume(","))
          break;
      }
      expect(")", "after arguments", "function call");
    }
    return node;
  }
}
