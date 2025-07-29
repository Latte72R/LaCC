
#include "lacc.h"

extern Token *token;
extern int label_cnt;
extern int loop_cnt;
extern int logical_cnt;
extern String *strings;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *expr() { return assign(); }

Node *assign_sub(Node *lhs, Node *rhs, char *ptr) {
  if (lhs->type->is_const) {
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
  Struct *is_struct;
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
      tok = expect_ident("struct reference");
      is_struct = node->type->is_struct;
      if (!is_struct) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!is_struct->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(is_struct, tok);
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
      tok = expect_ident("struct reference");
      is_struct = node->type->ptr_to->is_struct;
      if (!is_struct) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!is_struct->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(is_struct, tok);
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
    return new_num(expect_number("primary"));
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

  tok = expect_ident("primary");

  // enumのメンバー
  LVar *member = find_enum_member(tok);
  if (member) {
    node = new_num(member->offset);
    return node;
  }

  // 変数
  if (!consume("(")) {
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

int compile_time_number() {
  int result;
  if (consume("(")) {
    result = compile_time_number();
    expect(")", "after expression", "compile time number");
  } else if (token->kind == TK_NUM) {
    result = expect_number("compile time number");
  } else if (token->kind == TK_IDENT) {
    LVar *member = find_enum_member(consume_ident());
    if (!member) {
      error_expected_at(token->str, "compile time constant", token->str, "compile time number");
    }
    result = member->offset;
  } else {
    error_expected_at(token->str, "compile time constant", token->str, "compile time number");
  }
  return result;
}
