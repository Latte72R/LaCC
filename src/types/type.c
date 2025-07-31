
#include "lacc.h"

extern Token *token;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Type *parse_base_type_internal(int should_consume) {
  Token *tok = token;
  Type *type = malloc(sizeof(Type));

  // const修飾子の処理
  if (token->kind == TK_CONST) {
    type->is_const = TRUE;
    token = token->next;
  } else {
    type->is_const = FALSE;
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
    type->is_const = TRUE;
    token = token->next;
  }

  if (!should_consume)
    token = tok;
  return type;
}

Type *check_base_type() { return parse_base_type_internal(FALSE); }

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

Type *consume_type() {
  Type *type = parse_base_type_internal(TRUE);
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

// 配列サイズ解析の共通処理
Type *parse_array_dimensions(Type *base_type) {
  Type *type = base_type;
  while (consume("[")) {
    type = new_type_arr(type, expect_number("array declaration"));
    expect("]", "after number", "array declaration");
  }
  return type;
}

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR || type->ty == TY_ARGARR; }
int is_number(Type *type) { return type->ty == TY_INT || type->ty == TY_CHAR; }

char *type_name(Type *type) {
  switch (type->ty) {
  case TY_INT:
    return "int";
  case TY_CHAR:
    return "char";
  case TY_PTR:
    return "pointer";
  case TY_ARR:
    return "array";
  case TY_ARGARR:
    return "argument array";
  case TY_VOID:
    return "void";
  case TY_STRUCT: {
    return "struct";
  }
  default:
    return "unknown type";
  }
}
