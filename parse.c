
#include "9cc.h"
#include <string.h>

//
// Parser
//

extern Node *code[100];
extern Token *token;
extern Function *functions;
extern Function *current_fn;
extern int labelseq;

// Consumes the current token if it matches `op`.
bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return false;
  Token *tok = token;
  token = token->next;
  return tok;
}

Token *consume_type() {
  if (token->kind != TK_TYPE)
    return false;
  Token *tok = token;
  token = token->next;
  return tok;
}

// Ensure that the current token is `op`.
void expect(char *op, char *err, char *st) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    error("expected \"%s\":\n  %s  [in %s statement]", op, err, st);
  token = token->next;
}

// Ensure that the current token is TK_NUM.
int expect_number() {
  if (token->kind != TK_NUM)
    error("expected a number but got \"%.*s\" [in expect_number]", token->len, token->str);
  int val = token->val;
  token = token->next;
  return val;
}

bool is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR; }

int get_type_size(Type *type) {
  // intなら4、ポインタなら8
  if (type->ty == TY_INT) {
    return 4;
  } else if (is_ptr_or_arr(type)) {
    return 8;
  } else {
    error("invalid type [in get_type_size]");
    return 0;
  }
}

int get_sizeof(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_PTR) {
    return 8;
  } else if (type->ty == TY_ARR) {
    return get_sizeof(type->ptr_to) * type->array_size;
  } else {
    error("invalid type [in get_sizeof]");
    return 0;
  }
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = current_fn->locals; var; var = var->next)
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

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->endline = false;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
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
    error("duplicated function name: %.*s", tok->len, tok->str);
  }
  fn = calloc(1, sizeof(Function));
  fn->next = functions;
  fn->name = tok->str;
  fn->len = tok->len;
  fn->locals = calloc(1, sizeof(LVar));
  fn->locals->offset = 0;
  fn->variable_cnt = 0;
  fn->type = type;
  functions = fn;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->name = tok->str;
  node->val = tok->len;
  node->fn = fn;
  if (!consume(")")) {
    for (int i = 0; i < 4; i++) {
      if (!consume_type()) {
        error("expected a type but got \"%.*s\" [in function definition]", token->len, token->str);
      }
      Type *type = calloc(1, sizeof(Type));
      type->ty = TY_INT;
      while (consume("*")) {
        Type *ptr = calloc(1, sizeof(Type));
        ptr->ty = TY_PTR;
        ptr->ptr_to = type;
        type = ptr;
      }
      Token *tok_lvar = consume_ident();
      if (!tok_lvar) {
        error("expected an identifier but got \"%.*s\" [in function "
              "definition]",
              token->len, token->str);
      }
      Node *nd_lvar = new_node(ND_LVAR);
      node->args[i] = nd_lvar;
      LVar *lvar = calloc(1, sizeof(LVar));
      lvar->next = fn->locals;
      lvar->name = tok_lvar->str;
      lvar->len = tok_lvar->len;
      lvar->offset = fn->locals->offset + get_sizeof(type);
      lvar->type = type;
      nd_lvar->offset = lvar->offset;
      nd_lvar->type = type;
      fn->locals = lvar;
      fn->variable_cnt++;
      if (!consume(","))
        break;
    }
    expect(")", "after arguments", "function definition");
  }
  if (!(token->kind == TK_RESERVED && !memcmp(token->str, "{", token->len))) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
  } else {
    node->body = stmt();
  }
  current_fn = prev_fn;
  return node;
}

Node *variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_lvar(tok);
  if (lvar) {
    error("duplicated variable name: %.*s", tok->len, tok->str);
  }
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_VARDEC;
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
  lvar->type = type;
  node->offset = lvar->offset;
  node->type = type;
  lvar->next = current_fn->locals;
  current_fn->locals = lvar;
  current_fn->variable_cnt++;
  if (consume("=")) {
    node->kind = ND_LVAR;
    node = new_binary(ND_ASSIGN, node, equality());
  }
  return node;
}

Node *stmt() {
  Node *node;
  if (consume("{")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = calloc(100, sizeof(Node));
    int i = 0;
    while (!(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
      node->body[i++] = *stmt();
    }
    node->body[i].kind = ND_NONE;
    expect("}", "after block", "block");
  } else if (token->kind == TK_TYPE) {
    // 変数宣言または関数定義
    token = token->next;
    Type *type = calloc(1, sizeof(Type));
    type->ty = TY_INT;
    while (consume("*")) {
      Type *ptr = calloc(1, sizeof(Type));
      ptr->ty = TY_PTR;
      ptr->ptr_to = type;
      type = ptr;
    }
    Token *tok = consume_ident();
    if (!tok) {
      error("expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
    }
    if (consume("(")) {
      // 関数定義
      node = function_definition(tok, type);
    } else {
      // 変数宣言
      node = variable_declaration(tok, type);
      expect(";", "after line", "variable declaration");
      node->endline = true;
    }
  } else if (token->kind == TK_IF) {
    token = token->next;
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    node->id = labelseq++;
    expect("(", "before condition", "if");
    node->cond = equality();
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
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    node->id = labelseq++;
    node->cond = equality();
    expect(")", "after equality", "while");
    node->then = stmt();
  } else if (token->kind == TK_FOR) {
    token = token->next;
    expect("(", "before initialization", "for");
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    node->id = labelseq++;
    node->init = expr();
    expect(";", "after initialization", "for");
    node->cond = equality();
    expect(";", "after condition", "for");
    node->inc = expr();
    expect(")", "after step expression", "for");
    node->then = stmt();
  } else if (token->kind == TK_RETURN) {
    token = token->next;
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->rhs = equality();
    expect(";", "after line", "return");
  } else {
    node = expr();
    expect(";", "after line", "expression");
    node->endline = true;
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
  Node *node = equality();
  if (consume("=")) {
    node = new_binary(ND_ASSIGN, node, equality());
  }
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
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

  for (;;) {
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
Node *new_add(Node *lhs, Node *rhs) {
  Node *node;
  Node *mul_node;
  // どちらもポインタならエラー
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error("invalid type: ptr + ptr [in new_add]");
  }
  // lhsがptr, rhsがintなら
  if (is_ptr_or_arr(lhs->type) && rhs->type->ty == TY_INT) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_type_size(lhs->type->ptr_to)));
    node = new_binary(ND_ADD, lhs, mul_node);
    node->type = lhs->type;
  }
  // lhsがint, rhsがptrなら
  else if (lhs->type->ty == TY_INT && is_ptr_or_arr(rhs->type)) {
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

Node *new_sub(Node *lhs, Node *rhs) {
  Node *node;
  Node *mul_node;

  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error("invalid type: ptr - ptr [in new_sub]");
  }
  // lhsがint, rhsがptrなら
  if (lhs->type->ty == TY_INT && is_ptr_or_arr(rhs->type)) {
    error("invalid type: int - ptr [in new_sub]");
  }
  // lhsがptr, rhsがintなら
  if (is_ptr_or_arr(lhs->type) && rhs->type->ty == TY_INT) {
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
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      node = new_add(node, mul());
    } else if (consume("-")) {
      node = new_sub(node, mul());
    } else {
      return node;
    }
  }
}

Type *resolve_type_mul(Type *left, Type *right) {
  if (is_ptr_or_arr(left) || is_ptr_or_arr(right)) {
    error("invalid type [in resolve_type_mul]");
  }
  if (left->ty == TY_INT && right->ty == TY_INT) {
    return new_type_int();
  }
  error("invalid type [in resolve_type_mul]");
  return NULL;
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_binary(ND_MUL, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type);
    } else if (consume("/")) {
      node = new_binary(ND_DIV, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type);
    } else if (consume("%")) {
      node = new_binary(ND_REM, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type);
    } else {
      return node;
    }
  }
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
    return primary();
  if (consume("-")) {
    node = new_binary(ND_SUB, new_num(0), primary());
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
    node = new_node(ND_DEREF);
    node->lhs = unary();
    if (!is_ptr_or_arr(node->lhs->type)) {
      error("invalid pointer dereference");
    }
    node->type = node->lhs->type->ptr_to;
    return node;
  }
  return primary();
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  if (consume("(")) {
    node = equality();
    expect(")", "after expression", "primary");
    return node;
  }

  Token *tok = consume_ident();
  // 数値
  if (!tok) {
    node = new_num(expect_number());
  }

  // 変数
  else if (!consume("(")) {
    node = new_node(ND_LVAR);
    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->offset = lvar->offset;
      node->type = lvar->type;
      if (consume("[")) {
        if (!is_ptr_or_arr(node->type)) {
          error("invalid array access [in primary]");
        }
        node = new_add(node, equality());
        expect("]", "after number", "array access");
        Node *nd_deref = new_node(ND_DEREF);
        nd_deref->lhs = node;
        node = nd_deref;
        node->type = node->lhs->type->ptr_to;
      }
    } else {
      error("undefined variable: %.*s", tok->len, tok->str);
    }
  }

  // 関数呼び出し
  else {
    Function *fn = find_fn(tok);
    if (!fn) {
      error("undefined function: %.*s", tok->len, tok->str);
    }
    node = new_node(ND_FUNCALL);
    node->name = tok->str;
    node->val = tok->len;
    node->id = labelseq++;
    node->type = fn->type;
    if (!consume(")")) {
      for (int i = 0; i < 4; i++) {
        node->args[i] = equality();
        if (!consume(","))
          break;
      }
      expect(")", "after arguments", "function call");
    }
  }

  return node;
}
