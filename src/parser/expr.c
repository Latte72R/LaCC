
#include "lacc.h"

extern Token *token;
extern int loop_cnt;
extern int logical_cnt;
extern Location *consumed_loc;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *expr() { return assign(); }

Node *assign_sub(Node *lhs, Node *rhs, Location *loc, int check_const) {
  if (lhs->type->is_const && check_const) {
    error_at(loc, "constant variable cannot be assigned [in assign_sub]");
  } else if (lhs->type->ty == TY_ARR) {
    error_at(loc, "array variable cannot be assigned [in assign_sub]");
  } else if (rhs->type->ty == TY_VOID) {
    error_at(loc, "void type cannot be assigned [in assign_sub]");
  } else if (!is_same_type(lhs->type, rhs->type)) {
    warning_at(loc, "incompatible %s to %s conversion [in assign_sub]", type_name(rhs->type), type_name(lhs->type));
  }
  Node *node = new_binary(ND_ASSIGN, lhs, rhs);
  return node;
}

Node *assign() {
  Node *node = logical_or();
  Location *loc;
  if (consume("=")) {
    loc = consumed_loc;
    node = assign_sub(node, expr(), loc, TRUE);
  } else if (consume("+=")) {
    loc = consumed_loc;
    node = assign_sub(node, new_binary(ND_ADD, node, expr()), loc, TRUE);
  } else if (consume("-=")) {
    loc = consumed_loc;
    node = assign_sub(node, new_binary(ND_SUB, node, expr()), loc, TRUE);
  } else if (consume("*=")) {
    loc = consumed_loc;
    node = assign_sub(node, new_binary(ND_MUL, node, expr()), loc, TRUE);
  } else if (consume("/=")) {
    loc = consumed_loc;
    node = assign_sub(node, new_binary(ND_DIV, node, expr()), loc, TRUE);
  } else if (consume("%=")) {
    loc = consumed_loc;
    node = assign_sub(node, new_binary(ND_MOD, node, expr()), loc, TRUE);
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
Node *new_add(Node *lhs, Node *rhs, Location *loc) {
  Node *node;
  Node *mul_node;
  // lhsがloc, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(loc, "invalid operands to binary expression [in new_add]");
  }
  // lhsがloc, rhsがintなら
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

Node *new_sub(Node *lhs, Node *rhs, Location *loc) {
  Node *node;
  Node *mul_node;

  // lhsがloc, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = lhs->type;
    node = new_binary(ND_DIV, node, new_num(get_sizeof(lhs->type->ptr_to)));
    node->type = new_type(TY_INT);
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(loc, "invalid operands to binary expression [in new_sub]");
  }
  // lhsがloc, rhsがintなら
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
  Location *consumed_loc_prev;
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      consumed_loc_prev = consumed_loc;
      node = new_add(node, mul(), consumed_loc_prev);
    } else if (consume("-")) {
      consumed_loc_prev = consumed_loc;
      node = new_sub(node, mul(), consumed_loc_prev);
    } else {
      break;
    }
  }
  return node;
}

Type *resolve_type_mul(Type *left, Type *right, Location *loc) {
  if (is_number(left) && is_number(right)) {
    return new_type(TY_INT);
  }
  error_at(loc, "invalid operands to binary expression [in resolve_type_mul]");
  return NULL;
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Location *consumed_loc_prev;
  Node *node = type_cast();

  for (;;) {
    if (consume("*")) {
      consumed_loc_prev = consumed_loc;
      node = new_binary(ND_MUL, node, type_cast());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_loc_prev);
    } else if (consume("/")) {
      consumed_loc_prev = consumed_loc;
      node = new_binary(ND_DIV, node, type_cast());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_loc_prev);
    } else if (consume("%")) {
      consumed_loc_prev = consumed_loc;
      node = new_binary(ND_MOD, node, type_cast());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_loc_prev);
    } else {
      break;
    }
  }
  return node;
}

Node *type_cast() {
  Token *tok = token;
  if (!consume("(")) {
    token = tok;
    return unary();
  }
  Type *type = consume_type(TRUE);
  if (!type) {
    token = tok;
    return unary();
  }
  if (!consume(")")) {
    error_at(token->loc, "expected ')' after type cast [in type_cast]");
  }
  Node *node = new_node(ND_TYPECAST);
  node->lhs = unary();
  node->type = type;
  if (type->ty == TY_VOID) {
    // void型へのキャストは評価のみ
    return node->lhs;
  } else if (type->ty == TY_VOID) {
    error_at(tok->loc, "cannot cast to void type [in type_cast]");
  } else if (type->ty == TY_STRUCT) {
    error_at(tok->loc, "cannot cast to struct type [in type_cast]");
  } else if (type->ty == TY_UNION) {
    error_at(tok->loc, "cannot cast to union type [in type_cast]");
  } else if (node->lhs->type->ty == TY_STRUCT) {
    error_at(tok->loc, "cannot cast from struct type [in type_cast]");
  } else if (node->lhs->type->ty == TY_UNION) {
    error_at(tok->loc, "cannot cast from union type [in type_cast]");
  } else if (type_size(node->lhs->type) > type_size(type)) {
    warning_at(tok->loc, "cast to smaller integer type [in type_cast]");
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
    Location *consumed_loc_prev = consumed_loc;
    node = unary();
    node = new_deref(node);
    if (!is_ptr_or_arr(node->lhs->type)) {
      error_at(consumed_loc_prev, "invalid pointer dereference");
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
  Location *loc;
  if (consume("++")) {
    loc = consumed_loc;
    node = access_member();
    return assign_sub(node, new_add(node, new_num(1), consumed_loc), loc, TRUE);
  } else if (consume("--")) {
    loc = consumed_loc;
    node = access_member();
    return assign_sub(node, new_sub(node, new_num(1), consumed_loc), loc, TRUE);
  }
  node = access_member();
  if (consume("++")) {
    node = new_binary(ND_POSTINC, node, new_add(node, new_num(1), consumed_loc));
  } else if (consume("--")) {
    node = new_binary(ND_POSTINC, node, new_sub(node, new_num(1), consumed_loc));
  }
  return node;
}

// Objecture Reference and Array Indexing
Node *access_member() {
  Node *ptr_node;
  Node *offset_node;
  Token *tok;
  Object *object;
  LVar *var;
  Location *consumed_loc_prev;
  Token *prev_tok = token;
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      consumed_loc_prev = consumed_loc;
      if (!is_ptr_or_arr(node->type)) {
        error_at(consumed_loc_prev, "invalid array access [in primary]");
      }
      node = new_add(node, expr(), consumed_loc_prev);
      expect("]", "after number", "array access");
      node = new_deref(node);
    } else if (consume(".")) {
      if (node->type->ty != TY_STRUCT && node->type->ty != TY_UNION) {
        error_at(prev_tok->loc, "%.*s is not an object [in object reference]", prev_tok->len, prev_tok->str);
      }
      object = node->type->object;
      if (!object) {
        error_at(prev_tok->loc, "unknown object: %.*s [in object reference]", prev_tok->len, prev_tok->str);
      } else if (!object->is_defined) {
        error_at(prev_tok->loc, "incomplete definition of type [in object reference]");
      }
      tok = expect_ident("object reference");
      var = find_object_member(object, tok);
      offset_node = new_num(var->offset);
      ptr_node = new_node(ND_ADDR);
      ptr_node->lhs = node;
      ptr_node->type = new_type_ptr(node->type);
      ptr_node = new_binary(ND_ADD, ptr_node, offset_node);
      ptr_node->type = new_type_ptr(var->type);
      node = new_deref(ptr_node);
    } else if (consume("->")) {
      if (node->type->ty != TY_PTR) {
        error_at(prev_tok->loc, "%.*s is not a pointer [in struct reference]", prev_tok->len, prev_tok->str);
      }
      if (node->type->ptr_to->ty != TY_STRUCT && node->type->ptr_to->ty != TY_UNION) {
        error_at(prev_tok->loc, "%.*s is not a pointer of object [in struct reference]", prev_tok->len, prev_tok->str);
      }
      object = node->type->ptr_to->object;
      if (!object) {
        error_at(prev_tok->loc, "unknown object: %.*s [in object reference]", prev_tok->len, prev_tok->str);
      } else if (!object->is_defined) {
        error_at(prev_tok->loc, "incomplete definition of type [in object reference]", prev_tok->len, prev_tok->str);
      }
      tok = expect_ident("object reference");
      var = find_object_member(object, tok);
      offset_node = new_num(var->offset);
      ptr_node = new_binary(ND_ADD, node, offset_node);
      ptr_node->type = new_type_ptr(var->type);
      node = new_deref(ptr_node);
    } else
      break;
  }
  return node;
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  Token *tok;

  // 括弧
  if (consume("(")) {
    node = expr();
    expect(")", "after expression", "primary");
    return node;
  }

  // 数値
  if (token->kind == TK_NUM) {
    return new_num(expect_number("primary"));
  }

  // 文字列
  if (token->kind == TK_STRING) {
    String *str = string_literal();
    node = new_node(ND_STRING);
    node->id = str->id;
    node->type = new_type_ptr(new_type(TY_CHAR));
    return node;
  }

  // 型
  if (is_type(token)) {
    Type *type = consume_type(TRUE);
    node = new_node(ND_TYPE);
    node->type = type;
    return node;
  }

  tok = expect_ident("primary");

  // enumのメンバー
  LVar *member = find_enum_member(tok);
  if (member) {
    node = new_num(member->offset);
    return node;
  }

  // 変数
  if (!peek("(")) {
    LVar *lvar = find_lvar(tok);
    LVar *gvar = find_gvar(tok);
    if (lvar) {
      node = new_node(ND_LVAR);
      node->var = lvar;
      node->type = lvar->type;
    } else if (gvar) {
      node = new_node(ND_GVAR);
      node->var = gvar;
      node->type = gvar->type;
    } else {
      Function *fn = find_fn(tok);
      if (fn) {
        node = new_node(ND_FUNCNAME);
        node->fn = fn;
        node->type = new_type_ptr(fn->type);
      } else {
        error_at(tok->loc, "undefined variable: %.*s [in primary]", tok->len, tok->str);
      }
    }
    return node;
  }

  // 関数呼び出し
  else {
    consume("(");
    Location *loc = consumed_loc;
    Function *fn = find_fn(tok);
    if (!fn) {
      error_at(tok->loc, "undefined function: %.*s [in primary]", tok->len, tok->str);
    }
    node = new_node(ND_FUNCALL);
    node->fn = fn;
    node->id = loop_cnt++;
    node->type = fn->type->return_type;
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
    if (!fn->type_check) {
    } else if (node->val > fn->type->param_count) {
      error_at(loc, "too many arguments to function call: %.*s [in primary]", tok->len, tok->str);
    } else if (node->val < fn->type->param_count) {
      error_at(loc, "not enough arguments to function call: %.*s [in primary]", tok->len, tok->str);
    } else {
      for (int i = 0; i < node->val; i++) {
        if (!is_same_type(fn->type->param_types[i], node->args[i]->type)) {
          warning_at(loc, "incompatible %s to %s conversion [in primary]", type_name(node->args[i]->type),
                     type_name(fn->type->param_types[i]));
          break;
        }
      }
    }

    return node;
  }
}

int compile_time_number() {
  int result;
  if (consume("(")) {
    result = compile_time_number();
    expect(")", "after expression", "compile time number");
  } else if (token->kind == TK_IDENT) {
    LVar *member = find_enum_member(consume_ident());
    if (!member) {
      error_at(token->loc, "expected a compile time constant [in compile time number]");
    }
    result = member->offset;
  } else {
    result = expect_signed_number("compile time number");
  }
  return result;
}
