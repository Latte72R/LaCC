
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
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
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
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error("expected \"%s\":\n  %s  [in %s statement]", op, err, st);
  token = token->next;
}

// Ensure that the current token is TK_NUM.
int expect_number() {
  if (token->kind != TK_NUM)
    error("expected a number but got \"%.*s\" [in expect_number]", token->len,
          token->str);
  int val = token->val;
  token = token->next;
  return val;
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

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *function_definition(Token *tok) {
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
        error("expected a type but got \"%.*s\" [in function definition]",
              token->len, token->str);
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
      lvar->offset = i * 8 + 8;
      nd_lvar->offset = lvar->offset;
      fn->locals = lvar;
      fn->variable_cnt++;
      if (!consume(","))
        break;
    }
    expect(")", "after arguments", "function definition");
  }
  node->body = stmt();
  current_fn = prev_fn;
  return node;
}

Node *variable_declaration(Token *tok) {
  LVar *lvar = find_lvar(tok);
  if (lvar) {
    error("duplicated variable name: %.*s", tok->len, tok->str);
  }
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_LVAR;
  lvar = calloc(1, sizeof(LVar));
  lvar->next = current_fn->locals;
  lvar->name = tok->str;
  lvar->len = tok->len;
  lvar->offset = current_fn->locals->offset + 8;
  node->offset = lvar->offset;
  current_fn->locals = lvar;
  current_fn->variable_cnt++;
  if (consume("=")) {
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
    while (token->kind != TK_RESERVED && memcmp(token->str, "}", token->len)) {
      node->body[i++] = *stmt();
    }
    node->body[i].kind = ND_NONE;
    expect("}", "after block", "block");
  } else if (token->kind == TK_EXTERN) {
    node = new_node(ND_EXTERN);
    token = token->next;
    if (!consume_type()) {
      error("expected a type but got \"%.*s\" [in extern statement]",
            token->len, token->str);
    }
    Token *tok = consume_ident();
    if (!tok) {
      error("expected an identifier but got \"%.*s\" [in extern statement]",
            token->len, token->str);
    }
    expect("(", "after function name", "extern");
    Function *fn = calloc(1, sizeof(Function));
    fn->next = functions;
    fn->name = tok->str;
    fn->len = tok->len;
    functions = fn;
    if (!consume(")")) {
      for (int i = 0; i < 4; i++) {
        if (!consume_ident())
          break;
        if (!consume(","))
          break;
      }
      expect(")", "after arguments", "extern");
    }
    expect(";", "after line", "extern");
  } else if (token->kind == TK_TYPE) {
    // 変数宣言または関数定義
    token = token->next;
    Token *tok = consume_ident();
    if (!tok) {
      error("expected an identifier but got \"%.*s\" [in variable declaration]",
            token->len, token->str);
    }
    if (consume("(")) {
      // 関数定義
      node = function_definition(tok);
    } else {
      // 変数宣言
      node = variable_declaration(tok);
      expect(";", "after line", "variable declaration");
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
  Node *node;
  if (token->kind == TK_IDENT && token->next &&
      token->next->kind == TK_RESERVED &&
      !memcmp(token->next->str, "=", token->next->len)) {
    Token *tok = consume_ident();
    node = new_node(ND_LVAR);
    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->offset = lvar->offset;
    } else {
      error("undefined variable: %.*s", tok->len, tok->str);
    }
    consume("=");
    node = new_binary(ND_ASSIGN, node, equality());
  } else {
    node = equality();
  }
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_binary(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_binary(ND_NE, node, relational());
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_binary(ND_LT, node, add());
    else if (consume("<="))
      node = new_binary(ND_LE, node, add());
    else if (consume(">"))
      node = new_binary(ND_LT, add(), node);
    else if (consume(">="))
      node = new_binary(ND_LE, add(), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_binary(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_binary(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*"))
      node = new_binary(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_binary(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = ("+" | "-")? unary
//       | primary
Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_binary(ND_SUB, new_num(0), primary());
  if (consume("&")) {
    Node *node = new_node(ND_ADDR);
    node->rhs = unary();
    return node;
  }
  if (consume("*")) {
    Node *node = new_node(ND_DEREF);
    node->rhs = unary();
    return node;
  }
  return primary();
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  if (consume("(")) {
    node = expr();
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
