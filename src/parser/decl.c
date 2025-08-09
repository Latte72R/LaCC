
#include "lacc.h"

extern Token *token;
extern int array_cnt;
extern int block_id;
extern Function *functions;
extern Function *current_fn;
extern LVar *globals;
extern LVar *statics;
extern Object *structs;
extern ObjectTag *struct_tags;
extern Object *unions;
extern ObjectTag *union_tags;
extern Object *enums;
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
  Array *array = array_literal(type, org_type);
  Node *arr_node = new_node(ND_ARRAY);
  arr_node->type = type;
  arr_node->id = array->id;
  if (type->ty == TY_ARR) {
    if (type->array_size == 0) {
      // サイズ指定なしの文字列
      type->array_size = array->len;
    } else if (type->array_size < array->len) {
      warning_at(consumed_ptr, "excess elements in array initializer [in variable declaration]");
    }
    array->len = type->array_size;
    node = new_binary(ND_ASSIGN, node, arr_node);
    node->type = type;
    node->val = TRUE;
  } else if (type->ty == TY_PTR) {
    node = assign_sub(node, arr_node, consumed_ptr, FALSE);
    node->type = type;
  } else {
    error_at(consumed_ptr, "array initializer is only allowed for array or pointer type [in variable declaration]");
  }
  return node;
}

Node *handle_string_initialization(Node *node, Type *type, char *ptr) {
  String *str = string_literal();
  Node *string_node = new_node(ND_STRING);
  string_node->id = str->id;
  string_node->type = new_type_ptr(new_type(TY_CHAR));
  if (type->ty == TY_ARR) {
    if (type->array_size == 0) {
      // サイズ指定なしの文字列
      type->array_size = str->len;
    } else if (type->array_size < str->len) {
      warning_at(ptr, "initializer-string for char array is too long [in variable declaration]");
    } else if (type->ptr_to->ty != TY_CHAR) {
      error_at(ptr, "initializing wide char array with non-wide string literal [in variable declaration]");
    }
    node = new_binary(ND_ASSIGN, node, string_node);
    node->type = type;
    node->val = TRUE;
  } else if (type->ty == TY_PTR) {
    node = assign_sub(node, string_node, ptr, FALSE);
    node->type = type;
  } else {
    error_at(ptr, "string literal is only allowed for array or pointer type [in variable declaration]");
  }
  return node;
}

Node *handle_scalar_initialization(Node *node, Type *type, char *ptr) {
  node = assign_sub(node, expr(), ptr, FALSE);
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
  char *ptr = consumed_ptr;
  if (set_offset) {
    lvar->offset = expect_signed_number();
    return node;
  }
  if (peek("{")) {
    node = handle_array_initialization(node, type, org_type);
  } else if (token->kind == TK_STRING) {
    node = handle_string_initialization(node, type, ptr);
  } else {
    node = handle_scalar_initialization(node, type, ptr);
  }
  return node;
}

Node *function_definition(Token *tok, Type *type, int is_static) {
  Function *fn = find_fn(tok);
  if (fn && fn->is_defined) {
    error_at(consumed_ptr, "duplicated function definition: %.*s [in function definition]", tok->len, tok->str);
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
  fn->return_type = type;
  fn->is_defined = FALSE;
  fn->type_check = TRUE;
  fn->labels = malloc(sizeof(Label));
  fn->labels->next = NULL;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->fn = fn;
  int n = 0;
  for (int i = 0; i < 6; i++) {
    if (consume("...")) {
      // 可変長引数
      fn->type_check = FALSE;
      break;
    }
    type = consume_type(TRUE);
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
    fn->param_types[n] = type;
    nd_lvar->var = lvar;
    nd_lvar->type = type;
    fn->locals = lvar;
    n += 1;
    if (!consume(","))
      break;
  }
  fn->param_count = n;
  node->val = n;
  expect(")", "after arguments", "function definition");

  if (!peek("{")) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
    if (fn->param_count == 0) {
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
  Token *tok = token;
  Type *type = consume_type(FALSE);

  Node *node;
  if (token->kind != TK_IDENT) {
    // expression として扱う
    token = tok;
    return expression_stmt();
  }

  token = tok;
  Type *ch_type = check_base_type();
  type = consume_type(TRUE);
  tok = expect_ident("variable declaration");

  // 関数定義
  if (consume("(")) {
    if (current_fn->next) {
      error_at(token->str, "nested function is not supported [in function definition]");
    } else {
      return function_definition(tok, type, is_static);
    }
  }

  node = new_node(ND_BLOCK);
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

Node *struct_and_union_declaration(const int is_struct, const int is_union, const int should_record) {
  if (is_struct && is_union) {
    error_at(token->str, "cannot declare both struct and union at the same time [in object declaration]");
  } else if (!is_struct && !is_union) {
    error_at(token->str, "expected struct or union [in object declaration]");
  }
  Token *tok = expect_ident("object declaration");
  Node *node;
  Object *object = NULL;
  if (is_struct) {
    node = new_node(ND_STRUCT);
    if (tok && should_record) {
      object = find_struct(tok);
    }
  } else {
    node = new_node(ND_UNION);
    if (tok && should_record) {
      object = find_union(tok);
    }
  }
  if (object && object->is_defined && peek("{")) {
    error_at(tok->str, "duplicate object declaration: %.*s [in object declaration]", tok->len, tok->str);
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
    node->type = new_type_struct(object);
    node->endline = TRUE;
    return node;
  }
  object->is_defined = TRUE;
  object->var = malloc(sizeof(LVar));
  object->var->next = NULL;
  object->var->type = new_type(TY_NONE);
  int offset = 0;
  int max_size = 0;
  expect("{", "before object members", "object");
  while (!consume("}")) {
    Type *type = consume_type(TRUE);
    Type *org_type = type;
    do {
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
      error_at(token->str, "expected ';' after object member declaration [in object declaration]");
    }
  }
  if (is_struct) {
    // 構造体のサイズはメンバーの合計サイズ
    if (offset % max_size != 0) {
      offset += max_size - (offset % max_size);
    }
    object->size = offset;
    node->type = new_type_struct(object);
  } else {
    // unionのサイズは最大のメンバーのサイズ
    object->size = max_size;
    node->type = new_type_union(object);
  }
  return node;
}

Node *struct_stmt(const int should_record) {
  token = token->next;
  Node *node = struct_and_union_declaration(TRUE, FALSE, should_record);
  return node;
}

Node *union_stmt(const int should_record) {
  token = token->next;
  Node *node = struct_and_union_declaration(FALSE, TRUE, should_record);
  return node;
}

Node *typedef_stmt() {
  token = token->next;
  Node *node;
  if (token->kind == TK_STRUCT || token->kind == TK_UNION) {
    if (token->kind == TK_STRUCT) {
      token = token->next;
      node = struct_and_union_declaration(TRUE, FALSE, TRUE);
    } else if (token->kind == TK_UNION) {
      token = token->next;
      node = struct_and_union_declaration(FALSE, TRUE, TRUE);
    } else {
      error_at(token->str, "expected struct or union [in typedef]");
    }
    Type *type = node->type;
    node = new_node(ND_TYPEDEF);
    Token *tok = expect_ident("typedef");
    if (find_struct_tag(tok) || find_union_tag(tok)) {
      error_duplicate_name(tok, "typedef");
    }
    ObjectTag *tag = malloc(sizeof(ObjectTag));
    tag->name = tok->str;
    tag->len = tok->len;
    tag->object = type->object;
    if (type->ty == TY_STRUCT) {
      tag->next = struct_tags;
      struct_tags = tag;
    } else {
      tag->next = union_tags;
      union_tags = tag;
    }
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
    Object *enum_ = malloc(sizeof(Object));
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
