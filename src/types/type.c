
#include "lacc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern Token *token;
extern Location *consumed_loc;

// 前方宣言: 式パーサ
extern Node *expr();

Type *new_type(TypeKind ty) {
  Type *type = malloc(sizeof(Type));
  register_type(type);
  type->ty = ty;
  type->ptr_to = NULL;
  type->array_size = 0;
  type->object = NULL;
  type->is_const = false;
  type->is_unsigned = false;
  type->return_type = NULL;
  for (int i = 0; i < 6; i++) {
    type->param_types[i] = NULL;
    type->param_names[i] = NULL;
  }
  type->param_count = 0;
  type->is_variadic = false;
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
    return false;
  token = token->next;
  return true;
}

Type *parse_base_type_internal(const int should_consume, const int should_record) {
  Token *tok = token;
  Type *type = new_type(TY_NONE);

  // GCC 拡張: __extension__ は警告抑制用キーワード。型文脈では無視する。
  while (token->kind == TK_IDENT && token->len == (int)strlen("__extension__") &&
         strncmp(token->str, "__extension__", token->len) == 0) {
    token = token->next;
    tok = token;
  }

  // const修飾子の処理
  if (token->kind == TK_CONST) {
    type->is_const = true;
    token = token->next;
  } else {
    type->is_const = false;
  }

  // 基本型の処理
  if (token->kind == TK_STRUCT) {
    type->ty = TY_STRUCT;
    type->object = struct_and_union_declaration(true, false, should_record);
  } else if (token->kind == TK_UNION) {
    type->ty = TY_UNION;
    type->object = struct_and_union_declaration(false, true, should_record);
  } else if (token->kind == TK_ENUM) {
    type->ty = TY_INT;
    type->object = enum_declaration(should_record);
  } else if (token->kind == TK_IDENT) {
    // サポートしていないが、システムヘッダで現れる組込み型に対する最低限の対応
    // 例: glibc の stdarg.h などで使用される '__builtin_va_list'
    if (token->len == (int)strlen("__builtin_va_list") && strncmp(token->str, "__builtin_va_list", token->len) == 0) {
      // 内部表現としては void* 相当で十分（実際のレイアウトは不要）
      Type *void_ty = new_type(TY_VOID);
      type = new_type_ptr(void_ty);
      token = token->next;
    } else if (token->len == (int)strlen("wchar_t") && strncmp(token->str, "wchar_t", token->len) == 0) {
      // C では本来 typedef だが、ヘッダ互換性のためビルトイン扱い（LP64 では int 相当）
      type->ty = TY_INT;
      type->is_unsigned = false;
      token = token->next;
    } else if (token->len == (int)strlen("size_t") && strncmp(token->str, "size_t", token->len) == 0) {
      // size_t をビルトイン型として扱う（LP64 では unsigned long）
      type->ty = TY_LONG;
      type->is_unsigned = true;
      token = token->next;
    } else {
      TypeTag *type_tag = find_type_tag(token);
      if (type_tag) {
        type_tag->type->is_const = type->is_const;
        type = type_tag->type;
        token = token->next;
      } else {
        return NULL;
      }
    }
  } else if (token->kind != TK_TYPE) {
    return NULL;
  } else {
    // 任意順序の基本型指定子に対応する（例: "long unsigned int" や "unsigned long" 等）
    int seen_any = false;
    int long_count = 0;
    int seen_short = false;
    int seen_char = false;
    int seen_int = false;
    int seen_void = false;
    int seen_signed = false;
    int seen_float = false;
    int seen_double = false;

    for (;;) {
      if (consume_base_type("unsigned")) {
        type->is_unsigned = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("signed")) {
        seen_signed = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("long")) {
        long_count++;
        seen_any = true;
        continue;
      }
      if (consume_base_type("short")) {
        seen_short = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("char")) {
        seen_char = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("int")) {
        seen_int = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("float")) {
        seen_float = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("double")) {
        seen_double = true;
        seen_any = true;
        continue;
      }
      if (consume_base_type("void")) {
        seen_void = true;
        seen_any = true;
        break; // void は他の整数型指定子と組み合わせ不可
      }
      break; // 型指定子の並びが終了
    }

    if (!seen_any) {
      error_at(token->loc, "unknown type: %.*s [in parse_base_type]", token->len, token->str);
    }

    if (seen_void) {
      // void は単独扱い
      type->ty = TY_VOID;
      type->is_unsigned = false;
      if (seen_signed)
        error_at(tok->loc, "invalid type specifier combination [signed with void] [in parse_base_type]");
      if (seen_float || seen_double)
        error_at(tok->loc, "invalid type specifier combination [float/double with void] [in parse_base_type]");
    } else if (seen_char) {
      // (unsigned) char
      if (long_count || seen_short) {
        error_at(tok->loc, "invalid type specifier combination [char with long/short] [in parse_base_type]");
      }
      type->ty = TY_CHAR;
      if (seen_signed && type->is_unsigned)
        error_at(tok->loc, "conflicting type specifiers: signed and unsigned [in parse_base_type]");
      // 明示的に signed があっても is_unsigned は既定の false のまま
      if (seen_float || seen_double)
        error_at(tok->loc, "invalid type specifier combination [char with float/double] [in parse_base_type]");
    } else if (seen_short) {
      // (unsigned) short (int)
      if (long_count) {
        error_at(tok->loc, "invalid type specifier combination [short with long] [in parse_base_type]");
      }
      type->ty = TY_SHORT;
      if (seen_signed && type->is_unsigned)
        error_at(tok->loc, "conflicting type specifiers: signed and unsigned [in parse_base_type]");
      if (seen_float || seen_double)
        error_at(tok->loc, "invalid type specifier combination [short with float/double] [in parse_base_type]");
    } else if (long_count >= 2) {
      // (unsigned) long long (int)
      type->ty = TY_LONGLONG;
      if (seen_signed && type->is_unsigned)
        error_at(tok->loc, "conflicting type specifiers: signed and unsigned [in parse_base_type]");
      if (seen_float)
        error_at(tok->loc, "invalid type specifier combination [long long with float] [in parse_base_type]");
      if (seen_double)
        error_at(tok->loc, "invalid type specifier combination [double with long long] [in parse_base_type]");
    } else if (long_count == 1) {
      // (unsigned) long (int) または long double（簡易: double と同等サイズに擬似マップ）
      if (seen_double) {
        // long double -> ここでは 8 バイト double 相当に擬似マップ
        type->ty = TY_LONG; // 8 bytes
      } else {
        type->ty = TY_LONG;
      }
      if (seen_signed && type->is_unsigned)
        error_at(tok->loc, "conflicting type specifiers: signed and unsigned [in parse_base_type]");
      if (seen_float)
        error_at(tok->loc, "invalid type specifier combination [long with float] [in parse_base_type]");
    } else {
      // float/double か、(unsigned) int または単に (unsigned)
      if (seen_float && seen_double) {
        error_at(tok->loc, "invalid type specifier combination [both float and double] [in parse_base_type]");
      }
      if (seen_float) {
        // 擬似マップ: float は 4 バイト（int 相当）
        type->ty = TY_INT;
        type->is_unsigned = false;
      } else if (seen_double) {
        // 擬似マップ: double は 8 バイト（long 相当）
        type->ty = TY_LONG;
        type->is_unsigned = false;
      } else {
        (void)seen_int; // 明示的な int の有無は同じ扱い
        type->ty = TY_INT;
      }
      if (seen_signed && type->is_unsigned)
        error_at(tok->loc, "conflicting type specifiers: signed and unsigned [in parse_base_type]");
    }
  }

  // 後続のconst
  if (token->kind == TK_CONST) {
    type->is_const = true;
    token = token->next;
  }

  if (!should_consume)
    token = tok;
  return type;
}

Type *peek_base_type() { return parse_base_type_internal(false, false); }

// ポインタ修飾子を解析
Type *parse_pointer_qualifiers(Type *base_type) {
  Type *type = base_type;
  while (consume("*")) {
    Type *ptr = new_type_ptr(type);
    if (token->kind == TK_CONST) {
      ptr->is_const = true;
      token = token->next;
    } else {
      ptr->is_const = false;
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
        func->is_variadic = true;
        break;
      }
      Type *param = consume_type(true);
      if (!param) {
        error_at(consumed_loc, "expected a type [in %s]", stmt);
        break;
      } else if (param->ty == TY_VOID && peek(")")) {
        if (i != 0)
          error_at(consumed_loc, "void type is only allowed for the first argument [in %s]", stmt);
        break;
      }
      Token *ptok = NULL;
      // Support full declarators in parameter, such as function pointers: void (*f)(void)
      if (peek("(") || peek("*")) {
        param = parse_declarator(param, &ptok, stmt);
      } else {
        ptok = consume_ident();
        param = parse_array_dimensions(param);
        // 引数の配列はポインタとして扱う
        if (param->ty == TY_ARR)
          param->ty = TY_ARGARR;
      }
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
int eval_const_expr(Node *node, int *ok) {
  if (!node) {
    *ok = false;
    return 0;
  }
  switch (node->kind) {
  case ND_NUM:
    return node->val;
  case ND_ADD: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l + r);
  }
  case ND_SUB: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l - r);
  }
  case ND_MUL: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l * r);
  }
  case ND_DIV: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2 && (r != 0);
    if (!*ok)
      return 0;
    return (int)(l / r);
  }
  case ND_MOD: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2 && (r != 0);
    if (!*ok)
      return 0;
    return (int)(l % r);
  }
  case ND_SHL: {
    int ok1 = true, ok2 = true;
    unsigned long long l = (unsigned long long)eval_const_expr(node->lhs, &ok1);
    unsigned long long r = (unsigned long long)eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l << r);
  }
  case ND_SHR: {
    int ok1 = true, ok2 = true;
    unsigned long long l = (unsigned long long)eval_const_expr(node->lhs, &ok1);
    unsigned long long r = (unsigned long long)eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l >> r);
  }
  case ND_BITAND: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l & r);
  }
  case ND_BITOR: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l | r);
  }
  case ND_BITXOR: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l ^ r);
  }
  case ND_EQ: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l == r);
  }
  case ND_NE: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l != r);
  }
  case ND_LT: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l < r);
  }
  case ND_LE: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)(l <= r);
  }
  case ND_AND: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)((l != 0) && (r != 0));
  }
  case ND_OR: {
    int ok1 = true, ok2 = true;
    long long l = eval_const_expr(node->lhs, &ok1);
    long long r = eval_const_expr(node->rhs, &ok2);
    *ok = ok1 && ok2;
    return (int)((l != 0) || (r != 0));
  }
  case ND_BITNOT: {
    int ok1 = true;
    long long v = eval_const_expr(node->lhs, &ok1);
    *ok = ok1;
    return (int)(~v);
  }
  case ND_NOT: {
    int ok1 = true;
    long long v = eval_const_expr(node->lhs, &ok1);
    *ok = ok1;
    return v ? 0 : 1;
  }
  case ND_TERNARY: {
    int okc = true;
    long long c = eval_const_expr(node->cond, &okc);
    if (!okc) {
      *ok = false;
      return 0;
    }
    if (c) {
      int okt = true;
      int v = eval_const_expr(node->then, &okt);
      *ok = okt;
      return v;
    } else {
      int oke = true;
      int v = eval_const_expr(node->els, &oke);
      *ok = oke;
      return v;
    }
  }
  case ND_TYPECAST: {
    int ok1 = true;
    long long v = eval_const_expr(node->lhs, &ok1);
    *ok = ok1;
    // 最低限、キャストは値をそのまま用いる（厳密な幅の切り詰めは不要）
    return (int)v;
  }
  default:
    *ok = false;
    return 0;
  }
}

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
    Node *dim_expr = expr();
    expect("]", "after array dimension", "array dimension");
    int ok = true;
    int dim = eval_const_expr(dim_expr, &ok);
    if (!ok) {
      error_at(consumed_loc, "expected a compile time constant [in array dimension]");
    }
    if (dim < 0) {
      error_at(consumed_loc, "array dimension must be non-negative [in array dimension]");
    }
    arr = new_type_arr(base_type, dim);
  }
  type = arr;
  while (consume("[")) {
    if (consume("]")) {
      type->ptr_to = new_type_arr(base_type, 0);
    } else {
      Node *dim_expr = expr();
      expect("]", "after array dimension", "array dimension");
      int ok = true;
      int dim = eval_const_expr(dim_expr, &ok);
      if (!ok) {
        error_at(consumed_loc, "expected a compile time constant [in array dimension]");
      }
      if (dim < 0) {
        error_at(consumed_loc, "array dimension must be non-negative [in array dimension]");
      }
      type->ptr_to = new_type_arr(base_type, dim);
    }
    type = type->ptr_to;
  }
  return arr;
}

Type *consume_type(const int should_record) {
  Type *type = parse_base_type_internal(true, should_record);
  if (!type)
    return NULL;
  type = parse_pointer_qualifiers(type);
  return type;
}

int is_type(Token *tok) {
  if (tok->kind == TK_TYPE)
    return true;
  if (tok->kind == TK_CONST)
    return true;
  if (token->kind == TK_STRUCT || token->kind == TK_UNION || token->kind == TK_ENUM) {
    return true;
  }
  if (tok->kind == TK_IDENT) {
    TypeTag *type_tag = find_type_tag(token);
    if (type_tag)
      return true;
  }
  return false;
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

int is_enum_type(Type *type) {
  // このコンパイラでは enum は内部的に TY_INT だが、enum 由来の型は object に対応する enum オブジェクトが入る。
  // よって TY_INT かつ object != NULL を enum とみなす。
  return type && type->ty == TY_INT && type->object != NULL;
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
    return true; // ポインタ型同士は常に代入可能
  } else if (lhs->ty == rhs->ty && lhs->is_unsigned == rhs->is_unsigned) {
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
      return lhs->object == rhs->object;
    }
    return true;
  } else if ((is_number(lhs) || is_ptr_or_arr(lhs)) && (is_number(rhs) || is_ptr_or_arr(rhs))) {
    return true;
  } else {
    return false;
  }
}

int is_type_compatible(Type *lhs, Type *rhs) {
  // int と char の比較は許容
  // void* と他のポインタ型の比較も許容
  if (is_ptr_or_arr(lhs) && is_ptr_or_arr(rhs)) {
    if (!(lhs->ptr_to->is_const) && (rhs->ptr_to->is_const)) {
      return false; // const 修飾子が異なる場合は同じ型ではない
    } else if (lhs->ptr_to->ty == TY_VOID || rhs->ptr_to->ty == TY_VOID) {
      return true;
    }
    return is_type_compatible(lhs->ptr_to, rhs->ptr_to);
  } else if (lhs->ty == rhs->ty && lhs->is_unsigned == rhs->is_unsigned) {
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
      return lhs->object == rhs->object;
    }
    return true;
  } else if (is_number(lhs) && is_number(rhs)) {
    return true;
  } else {
    return false;
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
    return true;
  } else {
    return false;
  }
}
