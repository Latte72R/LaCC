
#include "lacc.h"

extern Token *token;
extern int array_cnt;
extern int block_id;
extern Function *functions;
extern Function *current_fn;
extern LVar *globals;
extern LVar *statics;
extern Struct *structs;
extern StructTag *struct_tags;
extern Enum *enums;
extern LVar *enum_members;
extern Array *arrays;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *handle_array_initialization(Node *node, Type *type, Type *org_type) {
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
  return node;
}

Node *handle_scalar_initialization(Node *node, Type *type) {
  node = new_binary(ND_ASSIGN, node, expr());
  node->type = type;
  if (node->rhs->kind == ND_STRING && node->lhs->type->ty == TY_ARR) {
    node->val = node->lhs->type->ptr_to->ty == TY_CHAR;
  }
  return node;
}

// 変数初期化の共通処理
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, Type *org_type, int set_offset) {
  if (!consume("=")) {
    if (set_offset)
      lvar->offset = 0;
    return node;
  }
  if (set_offset) {
    lvar->offset = expect_signed_number();
    return node;
  }
  if (consume("{")) {
    node = handle_array_initialization(node, type, org_type);
  } else {
    node = handle_scalar_initialization(node, type);
  }
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
    if (token->kind == TK_ELLIPSIS) {
      token = token->next;
      break;
    }
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
    Token *tok_lvar = expect_ident("function definition");
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

  if (!peek("{")) {
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
  if (lvar && lvar->block == block_id) {
    error_duplicate_name(tok, "local variable declaration");
  }
  Node *node = new_node(ND_VARDEC);
  lvar = new_lvar(tok, type, is_static, FALSE);
  Type *org_type = type;
  type = parse_array_dimensions(type);
  if (is_static) {
    lvar->block = block_id;
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
  node = handle_variable_initialization(node, lvar, type, org_type, is_static);
  node->endline = TRUE;
  return node;
}

Node *global_variable_declaration(Token *tok, Type *type, int is_static) {
  LVar *lvar = find_gvar(tok);
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
  node = handle_variable_initialization(node, lvar, type, NULL, TRUE);
  node->endline = TRUE;
  return node;
}

Node *extern_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gvar(tok);
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

Node *vardec_and_funcdef_stmt(int is_static, int is_extern) {
  // 変数宣言または関数定義
  Type *ch_type = check_base_type();
  Type *type = consume_type();
  Token *tok = expect_ident("variable declaration");

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
  if (is_extern) {
    node->body[i++] = extern_variable_declaration(tok, type);
  } else if (current_fn->next) {
    node->body[i++] = local_variable_declaration(tok, type, is_static);
  } else {
    node->body[i++] = global_variable_declaration(tok, type, is_static);
  }

  // 追加の変数
  while (consume(",")) {
    type = parse_pointer_qualifiers(ch_type);
    tok = expect_ident("variable declaration");
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    if (is_extern) {
      node->body[i++] = extern_variable_declaration(tok, type);
    } else if (current_fn->next) {
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

Node *struct_stmt() {
  token = token->next;
  Token *tok = expect_ident("struct declaration");
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
    Token *member_tok = expect_ident("struct member declaration");
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

Node *typedef_stmt() {
  token = token->next;
  Node *node;
  if (token->kind == TK_STRUCT) {
    token = token->next;
    node = new_node(ND_TYPEDEF);
    Token *tok1 = expect_ident("typedef");
    Token *tok2 = expect_ident("typedef");
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
      Token *member_tok = expect_ident("typedef");
      LVar *member_var = new_lvar(member_tok, new_type(TY_INT), FALSE, FALSE);
      member_var->offset = offset++;
      member_var->next = enum_members;
      enum_members = member_var;
    } while (consume(","));
    expect("}", "after enum members", "enum");
    Token *tok = expect_ident("typedef");
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
