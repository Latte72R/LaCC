
#include "lacc.h"

extern Token *token;
extern int array_cnt;
extern int block_id;
extern Function *functions;
extern Function *current_fn;
extern LVar *locals;
extern LVar *globals;
extern LVar *statics;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern TypeTag *type_tags;
extern Location *consumed_loc;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *handle_array_initialization(Node *node, Type *type) {
  if (type->ty != TY_ARR) {
    error_at(token->loc, "array initializer is only allowed for array type [in variable declaration]");
  }
  Array *array = array_literal(type);
  Node *arr_node = new_node(ND_ARRAY);
  arr_node->type = type;
  arr_node->id = array->id;
  if (type->ty == TY_ARR) {
    if (type->array_size == 0) {
      // サイズ指定なしの文字列
      type->array_size = array->len;
    } else if (type->array_size < array->len) {
      warning_at(consumed_loc, "excess elements in array initializer [in variable declaration]");
    }
    array->len = type->array_size;
    node = new_binary(ND_ASSIGN, node, arr_node);
    node->type = type;
    node->val = TRUE;
  } else if (type->ty == TY_PTR) {
    node = assign_sub(node, arr_node, consumed_loc, FALSE);
    node->type = type;
  } else {
    error_at(consumed_loc, "array initializer is only allowed for array or pointer type [in variable declaration]");
  }
  return node;
}

Node *handle_string_initialization(Node *node, Type *type, Location *loc) {
  String *str = string_literal();
  Node *string_node = new_node(ND_STRING);
  string_node->id = str->id;
  string_node->type = new_type_ptr(new_type(TY_CHAR));
  if (type->ty == TY_ARR) {
    if (type->array_size == 0) {
      // サイズ指定なしの文字列
      type->array_size = str->len;
    } else if (type->array_size < str->len) {
      warning_at(loc, "initializer-string for char array is too long [in variable declaration]");
    } else if (type->ptr_to->ty != TY_CHAR) {
      error_at(loc, "initializing wide char array with non-wide string literal [in variable declaration]");
    }
    node = new_binary(ND_ASSIGN, node, string_node);
    node->type = type;
    node->val = TRUE;
  } else if (type->ty == TY_PTR) {
    node = assign_sub(node, string_node, loc, FALSE);
    node->type = type;
  } else {
    error_at(loc, "string literal is only allowed for array or pointer type [in variable declaration]");
  }
  return node;
}

Node *handle_scalar_initialization(Node *node, Type *type, Location *loc) {
  node = assign_sub(node, expr(), loc, FALSE);
  node->type = type;
  if (node->rhs->kind == ND_STRING && node->lhs->type->ty == TY_ARR) {
    node->val = node->lhs->type->ptr_to->ty == TY_CHAR;
  }
  return node;
}

// 変数初期化の共通処理
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, int set_offset) {
  if (!consume("=")) {
    if (set_offset)
      lvar->offset = 0;
    return node;
  }
  Location *loc = consumed_loc;
  if (set_offset) {
    lvar->offset = expect_signed_number();
    return node;
  }
  if (peek("{")) {
    node = handle_array_initialization(node, type);
  } else if (token->kind == TK_STRING) {
    node = handle_string_initialization(node, type, loc);
  } else {
    node = handle_scalar_initialization(node, type, loc);
  }
  return node;
}

Node *function_definition(Token *tok, Type *type, int is_static) {
  Function *fn = find_fn(tok);
  if (fn && fn->is_defined) {
    error_at(consumed_loc, "duplicated function definition: %.*s [in function definition]", tok->len, tok->str);
  } else {
    fn = malloc(sizeof(Function));
    fn->next = functions;
    functions = fn;
  }
  fn->name = tok->str;
  fn->len = tok->len;
  fn->offset = 0;
  fn->is_static = is_static;
  fn->type = type;
  fn->is_defined = FALSE;
  fn->type_check = !type->is_variadic;
  fn->labels = NULL;
  locals = NULL;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->fn = fn;
  int n = type->param_count;
  for (int i = 0; i < n; i++) {
    Token *tok_lvar = type->param_names[i];
    Type *ptype = type->param_types[i];
    Node *nd_lvar = new_node(ND_LVAR);
    node->args[i] = nd_lvar;
    LVar *lvar = new_lvar(tok_lvar, ptype, FALSE, FALSE);
    int base = 0;
    if (locals)
      base = locals->offset;
    lvar->next = locals;
    if (is_ptr_or_arr(ptype)) {
      lvar->offset = base + 8;
    } else {
      lvar->offset = base + get_sizeof(ptype);
    }
    fn->offset = lvar->offset;
    nd_lvar->var = lvar;
    nd_lvar->type = ptype;
    locals = lvar;
  }
  node->val = n;

  if (!peek("{")) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
    if (fn->type->param_count == 0) {
      // C99のprototypeでは、引数を省略可能
      fn->type_check = FALSE;
    }
  } else {
    node->lhs = stmt();
    fn->is_defined = TRUE;
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
  if (is_static) {
    lvar->block = block_id;
    LVar *static_lvar = new_lvar(tok, type, TRUE, FALSE);
    static_lvar->block = lvar->block;
    static_lvar->next = statics;
    statics = static_lvar;
  } else {
    int base = 0;
    if (locals)
      base = locals->offset;
    lvar->offset = base + get_sizeof(type);
    if (current_fn->offset < lvar->offset) {
      current_fn->offset = lvar->offset;
    }
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = locals;
  locals = lvar;

  // 要修正
  node = handle_variable_initialization(node, lvar, type, is_static);
  node->endline = TRUE;
  return node;
}

Node *global_variable_declaration(Token *tok, Type *type, int is_static) {
  LVar *lvar = find_gvar(tok);
  if (lvar) {
    error_duplicate_name(tok, "global variable declaration");
  }
  Node *node = new_node(ND_GLBDEC);
  lvar = new_lvar(tok, type, is_static, FALSE);
  lvar->next = globals;
  globals = lvar;
  node->var = lvar;
  node->type = type;

  // 要修正
  node = handle_variable_initialization(node, lvar, type, TRUE);
  node->endline = TRUE;
  return node;
}

Node *extern_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gvar(tok);
  Node *node = new_node(ND_EXTERN);
  if (lvar) {
    return node;
  }
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
  Token *prev_tok = token;
  Type *type = consume_type(FALSE);

  Node *node;
  if (consume(";")) {
    // expression として扱う
    token = prev_tok;
    return expression_stmt();
  }

  token = prev_tok;
  Type *base_type = parse_base_type_internal(TRUE, TRUE);
  prev_tok = token;

  Token *tok;
  type = parse_declarator(base_type, &tok, "variable declaration");

  if (type->ty == TY_FUNC) {
    if (current_fn)
      error_at(token->loc, "nested function is not supported [in function definition]");
    return function_definition(tok, type, is_static);
  }

  if (!is_extern && type->object && !type->object->is_defined) {
    error_at(tok->loc, "variable has incomplete type [in variable declaration]");
  }

  node = new_node(ND_BLOCK);
  node->body = NULL;
  int i = 0;

  for (;;) {
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    if (is_extern) {
      node->body[i++] = extern_variable_declaration(tok, type);
    } else if (current_fn) {
      node->body[i++] = local_variable_declaration(tok, type, is_static);
    } else {
      node->body[i++] = global_variable_declaration(tok, type, is_static);
    }
    if (!consume(","))
      break;
    type = parse_declarator(base_type, &tok, "variable declaration");
  }

  node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
  node->body[i] = new_node(ND_NONE);
  expect(";", "after line", "variable declaration");
  node->endline = TRUE;

  return node;
}

Object *struct_and_union_declaration(const int is_struct, const int is_union, const int should_record) {
  token = token->next;
  if (is_struct && is_union) {
    error_at(token->loc, "cannot declare both struct and union at the same time [in object declaration]");
  } else if (!is_struct && !is_union) {
    error_at(token->loc, "expected struct or union [in object declaration]");
  }
  Token *tok;
  if (token->kind != TK_IDENT) {
    tok = NULL;
  } else {
    tok = expect_ident("object declaration");
  }
  Object *object = NULL;
  if (tok && should_record) {
    if (is_struct) {
      object = find_struct(tok);
    } else {
      object = find_union(tok);
    }
  }
  if (object && object->is_defined && peek("{")) {
    error_at(tok->loc, "duplicate object declaration: %.*s [in object declaration]", tok->len, tok->str);
  } else if (!object) {
    object = malloc(sizeof(Object));
    object->is_defined = FALSE;
    if (tok) {
      object->name = tok->str;
      object->len = tok->len;
    } else {
      object->name = NULL;
      object->len = 0;
    }
    if (should_record) {
      if (is_struct) {
        object->next = structs;
        structs = object;
      } else {
        object->next = unions;
        unions = object;
      }
    }
  }
  if (!peek("{")) {
    return object;
  }
  object->is_defined = TRUE;
  object->var = NULL;
  int offset = 0;
  int max_size = 0;
  expect("{", "before object members", "object");
  while (!consume("}")) {
    Type *type = consume_type(TRUE);
    Type *org_type = type;
    do {
      /*
      Type *base_type = parse_base_type_internal(TRUE, TRUE);
      Token *prev_tok = token;
      Type *org_type = parse_pointer_qualifiers(base_type);
      token = prev_tok;
      Token *tok;
      Type *type = parse_declarator(base_type, &tok, "object declaration");
      type = parse_array_dimensions(type);
      */
      Token *member_tok = expect_ident("object member declaration");
      type = parse_array_dimensions(type);
      LVar *member_var = new_lvar(member_tok, type, FALSE, FALSE);
      member_var->next = object->var;
      object->var = member_var;
      int single_size = get_sizeof(org_type);
      if (offset % single_size != 0) {
        offset += single_size - (offset % single_size);
      }
      if (max_size < single_size) {
        max_size = single_size;
      }
      if (is_struct) {
        // 構造体のメンバーはオフセットを持つ
        member_var->offset = offset;
      } else {
        // unionのメンバーはオフセットを持たない
        member_var->offset = 0;
      }
      offset += get_sizeof(type);
    } while (consume(","));
    if (consume(";")) {
      continue;
    } else {
      error_at(token->loc, "expected ';' after object member declaration [in object declaration]");
    }
  }
  if (is_struct) {
    // 構造体のサイズはメンバーの合計サイズ
    if (offset % max_size != 0) {
      offset += max_size - (offset % max_size);
    }
    object->size = offset;
  } else {
    // unionのサイズは最大のメンバーのサイズ
    object->size = max_size;
  }
  return object;
}

Object *enum_declaration(const int should_record) {
  token = token->next;
  Token *tok;
  if (token->kind != TK_IDENT) {
    tok = NULL;
  } else {
    tok = expect_ident("enum declaration");
  }
  Object *object = NULL;
  if (tok && should_record) {
    object = find_enum(tok);
  }
  if (object && object->is_defined && peek("{")) {
    error_at(tok->loc, "duplicate object declaration: %.*s [in enum declaration]", tok->len, tok->str);
  } else if (!object) {
    object = malloc(sizeof(Object));
    object->is_defined = FALSE;
    object->var = NULL;
    if (tok) {
      object->name = tok->str;
      object->len = tok->len;
    } else {
      object->name = NULL;
      object->len = 0;
    }
    if (should_record) {
      object->next = enums;
      enums = object;
    }
  }
  if (!peek("{")) {
    return object;
  }
  object->is_defined = TRUE;

  int value = 0;
  expect("{", "before enum members", "enum");
  do {
    Token *member_tok = expect_ident("typedef");
    LVar *member_var = new_lvar(member_tok, new_type(TY_INT), FALSE, FALSE);
    member_var->offset = value++;
    member_var->next = object->var;
    object->var = member_var;
  } while (consume(","));
  expect("}", "after enum members", "enum");

  return object;
}

Node *typedef_stmt() {
  token = token->next;
  Node *node;
  Type *type = consume_type(TRUE);
  node = new_node(ND_TYPEDEF);
  Token *tok;
  type = parse_declarator(type, &tok, "typedef");
  node->type = type;
  TypeTag *tag = find_type_tag(tok);
  if (tag) {
    if (is_same_type(type, tag->type)) {
      expect(";", "after line", "typedef");
      return node;
    } else {
      error_at(consumed_loc, "typedef redefinition with different types [in typedef]");
    }
  }
  tag = malloc(sizeof(TypeTag));
  tag->name = tok->str;
  tag->len = tok->len;
  tag->type = type;
  tag->next = type_tags;
  type_tags = tag;
  expect(";", "after line", "typedef");
  node->endline = TRUE;
  return node;
}
