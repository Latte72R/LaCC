
#include "lacc.h"

extern Token *token;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Type *new_type(TypeKind ty) {
  Type *type = malloc(sizeof(Type));
  type->ty = ty;
  type->ptr_to = NULL;
  type->array_size = 0;
  type->object = NULL;
  type->is_const = FALSE;
  type->return_type = NULL;
  for (int i = 0; i < 6; i++) {
    type->param_types[i] = NULL;
  }
  type->param_count = 0;
  return type;
}

Type *new_type_ptr(Type *ptr_to) {
  Type *type = new_type(TY_PTR);
  type->ptr_to = ptr_to;
  type->array_size = 1;
  return type;
}

Type *new_type_arr(Type *ptr_to, int array_size) {
  Type *type = new_type(TY_ARR);
  type->ptr_to = ptr_to;
  type->array_size = array_size;
  return type;
}

Type *new_type_func(Type *ptr_to, int array_size) {
  Type *type = new_type(TY_ARR);
  type->ptr_to = ptr_to;
  type->array_size = array_size;
  return type;
}

Type *parse_base_type_internal(const int should_consume, const int should_record) {
  Token *tok = token;
  Type *type = new_type(TY_NONE);

  // const修飾子の処理
  if (token->kind == TK_CONST) {
    type->is_const = TRUE;
    token = token->next;
  } else {
    type->is_const = FALSE;
  }

  // 基本型の処理
  if (token->kind == TK_STRUCT) {
    type->ty = TY_STRUCT;
    type->object = struct_and_union_declaration(TRUE, FALSE, should_record);
  } else if (token->kind == TK_UNION) {
    type->ty = TY_UNION;
    type->object = struct_and_union_declaration(FALSE, TRUE, should_record);
  } else if (token->kind == TK_ENUM) {
    type->ty = TY_INT;
    type->object = enum_declaration(should_record);
  } else if (token->kind == TK_IDENT) {
    TypeTag *type_tag = find_type_tag(token);
    if (type_tag) {
      type_tag->type->is_const = type->is_const;
      type = type_tag->type;
    } else {
      return NULL;
    }
    token = token->next;
  } else if (token->kind != TK_TYPE) {
    return NULL;
  } else {
    type->ty = token->ty;
    token = token->next;
  }

  // 後続のconst
  if (token->kind == TK_CONST) {
    type->is_const = TRUE;
    token = token->next;
  }

  if (!should_consume)
    token = tok;
  return type;
}

Type *peek_base_type() { return parse_base_type_internal(FALSE, FALSE); }
/*
LVar *declaration();
LVar *declarator();
LVar *suffix();
LVar *pointer();
LVar *direct_decl();
LVar *abstruct_decl();

LVar *declaration() {
  Type *base = parse_base_type_internal(TRUE, TRUE);
  return declarator();
}

LVar *declarator() { return pointer(); }

LVar *pointer() {
  if (consume("*")) {
    LVar *var = suffix();
    var->type = new_type_ptr(var->type);
    return var;
  }
  return suffix();
}

LVar *suffix() {
  LVar *base = direct_decl(type);
  if (consume("(")) {
    type = new_type(TY_FUNC);
    type->return_type = base;
    int i = 0;
    do {
      type->param_types[i++] = abstruct_decl(base);
    } while (consume(","));
    expect(")", "after suffix", "suffix");
    return type;
  } else if (consume("[")) {
    type = new_type_arr(base, compile_time_number());
    expect("]", "after suffix", "suffix");
    return type;
  } else {
    return base;
  }
}

LVar *direct_decl() {
  if (consume("(")) {
    LVar *var = declarator();
    expect(")", "after suffix", "suffix");
    return var;
  }
  Token *tok = expect_ident("direct_decl");
  Type *type;
  if (consume("(")) {
    type = new_type(TY_FUNC);
    int i = 0;
    do {
      type->param_types[i] = declaration();
    } while (consume(","));
    expect(")", "after function arguments", "direct_decl");
  } else {
    type = malloc(sizeof(Type));
  }
  LVar *var = new_lvar(NULL, type, FALSE, FALSE);
  return var;
}

LVar *abstruct_decl() { return malloc(sizeof(LVar)); }
*/

// ポインタ修飾子を解析
Type *parse_pointer_qualifiers(Type *base_type) {
  Type *type = base_type;
  while (consume("*")) {
    Type *ptr = new_type_ptr(type);
    if (token->kind == TK_CONST) {
      ptr->is_const = TRUE;
      token = token->next;
    } else {
      ptr->is_const = FALSE;
    }
    type = ptr;
  }
  return type;
}

Type *consume_type(const int should_record) {
  Type *type = parse_base_type_internal(TRUE, should_record);
  if (!type)
    return NULL;
  type = parse_pointer_qualifiers(type);
  return type;
}

int is_type(Token *tok) {
  if (tok->kind == TK_TYPE)
    return TRUE;
  if (tok->kind == TK_CONST)
    return TRUE;
  if (token->kind == TK_STRUCT || token->kind == TK_UNION || token->kind == TK_ENUM) {
    return TRUE;
  }
  if (tok->kind == TK_IDENT) {
    TypeTag *type_tag = find_type_tag(token);
    if (type_tag)
      return TRUE;
  }
  return FALSE;
}

// 予約しているスタック領域のサイズ
int get_sizeof(Type *type) {
  if (type->object && !type->object->is_defined) {
    error_at(token->loc, "invalid application of 'sizeof' to an incomplete type [in get_sizeof]");
  }
  switch (type->ty) {
  case TY_INT:
    return 4;
  case TY_CHAR:
    return 1;
  case TY_PTR:
    return 8;
  case TY_ARR:
  case TY_ARGARR:
    return get_sizeof(type->ptr_to) * type->array_size;
  case TY_STRUCT:
  case TY_UNION:
    return type->object->size;
  default:
    error_at(token->loc, "invalid type [in get_sizeof]");
    return 0;
  }
}

int type_size(Type *type) {
  if (type->object && !type->object->is_defined) {
    error_at(token->loc, "incomplete definition of type [in type_size]");
  }
  switch (type->ty) {
  case TY_VOID:
    return 0;
  case TY_INT:
    return 4;
  case TY_CHAR:
    return 1;
  case TY_PTR:
  case TY_ARR:
  case TY_ARGARR:
    return 8;
  case TY_STRUCT:
  case TY_UNION:
    return type->object->size;
  default:
    error_at(token->loc, "invalid type [in type_size]");
    return 0;
  }
}

// 配列サイズ解析の共通処理
Type *parse_array_dimensions(Type *base_type) {
  Type *type;
  Type *arr;
  if (!consume("[")) {
    return base_type;
  }
  if (consume("]")) {
    // サイズ指定なしの配列
    arr = new_type_arr(base_type, 0);
  } else {
    arr = new_type_arr(base_type, expect_number("array dimension"));
  }
  expect("]", "after array dimension", "array dimension");
  type = arr;
  while (consume("[")) {
    type->ptr_to = new_type_arr(base_type, expect_number("array dimension"));
    type = type->ptr_to;
    expect("]", "after array dimension", "array dimension");
  }
  return arr;
}

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR || type->ty == TY_ARGARR; }
int is_number(Type *type) { return type->ty == TY_INT || type->ty == TY_CHAR; }

char *type_name(Type *type) {
  switch (type->ty) {
  case TY_INT:
  case TY_CHAR:
    return "integer";
  case TY_PTR:
    return "pointer";
  case TY_ARR:
  case TY_ARGARR:
    return "array";
  case TY_VOID:
    return "void";
  case TY_STRUCT:
    return "struct";
  case TY_UNION:
    return "union";
  default:
    return "unknown";
  }
}

int is_same_type(Type *lhs, Type *rhs) {
  // int と char の比較は許容
  // void* と他のポインタ型の比較も許容
  if (is_ptr_or_arr(lhs) && is_ptr_or_arr(rhs)) {
    if (!(lhs->ptr_to->is_const) && (rhs->ptr_to->is_const)) {
      return FALSE; // const 修飾子が異なる場合は同じ型ではない
    } else if (lhs->ptr_to->ty == TY_VOID || rhs->ptr_to->ty == TY_VOID) {
      return TRUE;
    }
    return is_same_type(lhs->ptr_to, rhs->ptr_to);
  } else if (lhs->ty == rhs->ty) {
    return TRUE;
  } else if (lhs->ty == TY_INT && rhs->ty == TY_CHAR) {
    return TRUE;
  } else if (lhs->ty == TY_CHAR && rhs->ty == TY_INT) {
    return TRUE;
  } else {
    return FALSE;
  }
}
