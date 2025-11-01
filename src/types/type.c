
#include "lacc.h"

extern Token *token;
extern Location *consumed_loc;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Type *new_type(TypeKind ty) {
  Type *type = malloc(sizeof(Type));
  register_type(type);
  type->ty = ty;
  type->ptr_to = NULL;
  type->array_size = 0;
  type->object = NULL;
  type->is_const = FALSE;
  type->is_unsigned = FALSE;
  type->return_type = NULL;
  for (int i = 0; i < 6; i++) {
    type->param_types[i] = NULL;
    type->param_names[i] = NULL;
  }
  type->param_count = 0;
  type->is_variadic = FALSE;
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

int consume_base_type(char *name) {
  if (token->kind != TK_TYPE || strcmp(token->str, name))
    return FALSE;
  token = token->next;
  return TRUE;
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
    if (consume_base_type("unsigned")) {
      type->is_unsigned = TRUE;
      if (consume_base_type("long")) {
        if (consume_base_type("long")) {
          type->ty = TY_LONGLONG;
        } else {
          type->ty = TY_LONG;
        }
      } else if (consume_base_type("short")) {
        type->ty = TY_SHORT;
      } else if (consume_base_type("char")) {
        type->ty = TY_CHAR;
      } else if (consume_base_type("int")) {
        type->ty = TY_INT;
      } else {
        type->ty = TY_INT;
      }
    } else if (consume_base_type("int")) {
      type->ty = TY_INT;
    } else if (consume_base_type("char")) {
      type->ty = TY_CHAR;
    } else if (consume_base_type("short")) {
      type->ty = TY_SHORT;
    } else if (consume_base_type("long")) {
      if (consume_base_type("long")) {
        type->ty = TY_LONGLONG;
      } else {
        type->ty = TY_LONG;
      }
    } else if (consume_base_type("void")) {
      type->ty = TY_VOID;
    } else {
      error_at(token->loc, "unknown type: %.*s [in parse_base_type]", token->len, token->str);
    }
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

// 関数宣言: 引数リストを解析して関数型を構築
Type *parse_function_suffix(Type *type, char *stmt) {
  consume("(");
  Type *func = new_type(TY_FUNC);
  func->return_type = type;
  int n = 0;
  if (!consume(")")) {
    for (int i = 0; i < 6; i++) {
      if (consume("...")) {
        func->is_variadic = TRUE;
        break;
      }
      Type *param = consume_type(TRUE);
      if (!param) {
        error_at(consumed_loc, "expected a type [in %s]", stmt);
        break;
      } else if (param->ty == TY_VOID) {
        if (i != 0)
          error_at(consumed_loc, "void type is only allowed for the first argument [in %s]", stmt);
        break;
      }
      Token *ptok = consume_ident();
      param = parse_array_dimensions(param);
      // 引数の配列はポインタとして扱う
      if (param->ty == TY_ARR)
        param->ty = TY_ARGARR;
      func->param_types[i] = param;
      func->param_names[i] = ptok;
      n += 1;
      if (!consume(","))
        break;
    }
    expect(")", "after arguments", stmt);
  }
  func->param_count = n;
  type = func;
  return type;
}

// 後ろに続く "()" や "[]" を解析し、関数・配列の型情報を再帰的に組み立てる
Type *parse_declarator_suffix(Type *type, char *stmt) {
  for (;;) {
    if (peek("(")) {
      // 関数宣言: 引数リストを解析して関数型を構築
      type = parse_function_suffix(type, stmt);
      continue;
    }
    if (peek("[")) {
      // 配列宣言: [] の連なりを処理
      type = parse_array_dimensions(type);
      continue;
    }
    return type; // 追加の修飾が無ければ確定
  }
}

// 仮置きした型 placeholder を、実際の型 actual で再帰的に置き換える
void substitute_type(Type *where, Type *placeholder, Type *actual) {
  // 末端に到達したら終了
  if (!where)
    return;
  // placeholder 自体を actual で置換
  if (where == placeholder) {
    *where = *actual;
    return;
  }
  // ポインタ先の型を再帰的に置換
  if (where->ptr_to) {
    if (where->ptr_to == placeholder)
      where->ptr_to = actual;
    else
      substitute_type(where->ptr_to, placeholder, actual);
  }
  // 関数の戻り値型を再帰的に置換
  if (where->return_type) {
    if (where->return_type == placeholder)
      where->return_type = actual;
    else
      substitute_type(where->return_type, placeholder, actual);
  }
}

// ベース型から宣言子全体を読み取り、識別子とポインタ・配列・関数などの修飾を解析して最終的な型を得る
Type *parse_declarator(Type *base_type, Token **tok, char *stmt) {
  // 先頭の "*" などポインタ修飾子を処理
  Type *type = parse_pointer_qualifiers(base_type);

  if (consume("(")) {
    // 括弧で囲まれた宣言子を再帰的に解析
    Type *placeholder = new_type(TY_NONE);
    Type *inner = parse_declarator(placeholder, tok, stmt);
    expect(")", "after declarator", stmt);
    Type *suffix = parse_declarator_suffix(type, stmt);
    substitute_type(inner, placeholder, suffix);
    return inner;
  }

  *tok = expect_ident(stmt);
  return parse_declarator_suffix(type, stmt);
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
    expect("]", "after array dimension", "array dimension");
  }
  type = arr;
  while (consume("[")) {
    if (consume("]")) {
      type->ptr_to = new_type_arr(base_type, 0);
    } else {
      type->ptr_to = new_type_arr(base_type, expect_number("array dimension"));
      expect("]", "after array dimension", "array dimension");
    }
    type = type->ptr_to;
  }
  return arr;
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
  case TY_SHORT:
    return 2;
  case TY_LONG:
  case TY_LONGLONG:
    return 8;
  case TY_PTR:
  case TY_ARGARR:
    return 8;
  case TY_ARR:
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
  case TY_SHORT:
    return 2;
  case TY_LONG:
  case TY_LONGLONG:
    return 8;
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

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR || type->ty == TY_ARGARR; }
int is_number(Type *type) {
  return type->ty == TY_INT || type->ty == TY_CHAR || type->ty == TY_SHORT || type->ty == TY_LONG ||
         type->ty == TY_LONGLONG;
}

static int type_rank(Type *type) {
  switch (type->ty) {
  case TY_CHAR:
    return 1;
  case TY_SHORT:
    return 2;
  case TY_INT:
    return 3;
  case TY_LONG:
    return 4;
  case TY_LONGLONG:
    return 5;
  default:
    return 0;
  }
}

Type *max_type(Type *lhs, Type *rhs) {
  int size_l = type_size(lhs);
  int size_r = type_size(rhs);
  TypeKind ty;
  if (size_l > size_r)
    ty = lhs->ty;
  else if (size_r > size_l)
    ty = rhs->ty;
  else
    ty = type_rank(lhs) >= type_rank(rhs) ? lhs->ty : rhs->ty;
  Type *t = new_type(ty);
  t->is_unsigned = lhs->is_unsigned || rhs->is_unsigned;
  return t;
}

char *type_name(Type *type) {
  if (type->is_unsigned) {
    switch (type->ty) {
    case TY_INT:
      return "unsigned int";
    case TY_CHAR:
      return "unsigned char";
    case TY_SHORT:
      return "unsigned short";
    case TY_LONG:
      return "unsigned long";
    case TY_LONGLONG:
      return "unsigned long long";
    default:
      break;
    }
  }
  switch (type->ty) {
  case TY_INT:
    return "int";
  case TY_CHAR:
    return "char";
  case TY_SHORT:
    return "short";
  case TY_LONG:
    return "long";
  case TY_LONGLONG:
    return "long long";
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

int is_type_assignable(Type *lhs, Type *rhs) {
  // int と char の比較は許容
  // void* と他のポインタ型の比較も許容
  if (is_ptr_or_arr(lhs) && is_ptr_or_arr(rhs)) {
    return TRUE; // ポインタ型同士は常に代入可能
  } else if (lhs->ty == rhs->ty && lhs->is_unsigned == rhs->is_unsigned) {
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
      return lhs->object == rhs->object;
    }
    return TRUE;
  } else if ((is_number(lhs) || is_ptr_or_arr(lhs)) && (is_number(rhs) || is_ptr_or_arr(rhs))) {
    return TRUE;
  } else {
    return FALSE;
  }
}

int is_type_compatible(Type *lhs, Type *rhs) {
  // int と char の比較は許容
  // void* と他のポインタ型の比較も許容
  if (is_ptr_or_arr(lhs) && is_ptr_or_arr(rhs)) {
    if (!(lhs->ptr_to->is_const) && (rhs->ptr_to->is_const)) {
      return FALSE; // const 修飾子が異なる場合は同じ型ではない
    } else if (lhs->ptr_to->ty == TY_VOID || rhs->ptr_to->ty == TY_VOID) {
      return TRUE;
    }
    return is_type_compatible(lhs->ptr_to, rhs->ptr_to);
  } else if (lhs->ty == rhs->ty && lhs->is_unsigned == rhs->is_unsigned) {
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
      return lhs->object == rhs->object;
    }
    return TRUE;
  } else if (is_number(lhs) && is_number(rhs)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

int is_type_identical(Type *lhs, Type *rhs) {
  // const 修飾子の有無を無視して型の同一性を確認
  if (is_ptr_or_arr(lhs) && is_ptr_or_arr(rhs)) {
    return is_type_identical(lhs->ptr_to, rhs->ptr_to);
  } else if (lhs->ty == rhs->ty && lhs->is_unsigned == rhs->is_unsigned) {
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
      return lhs->object == rhs->object;
    }
    return TRUE;
  } else {
    return FALSE;
  }
}
