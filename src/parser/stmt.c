
#include "lacc.h"

extern Token *token;
extern int label_cnt;
extern int loop_cnt;
extern int loop_id;
extern int block_cnt;
extern int block_id;
extern Function *current_fn;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *block_stmt() {
  Node *node = new_node(ND_BLOCK);
  LVar *locals_prev = current_fn->locals;
  int block_id_prev = block_id;
  block_id = block_cnt++;
  node->body = malloc(sizeof(Node *));
  int i = 0;
  while (!consume("}")) {
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    node->body[i++] = stmt();
  }
  node->body[i] = new_node(ND_NONE);
  current_fn->locals = locals_prev;
  block_id = block_id_prev;
  return node;
}

Node *goto_stmt() {
  token = token->next;
  Token *tok = expect_ident("goto statement");
  Node *node = new_node(ND_GOTO);
  node->label = malloc(sizeof(Label));
  node->label->name = tok->str;
  node->label->len = tok->len;
  node->fn = current_fn;
  node->endline = TRUE;
  return node;
}

Node *label_stmt() {
  Label *label = malloc(sizeof(Label));
  label->name = token->str;
  label->len = token->len;
  label->id = label_cnt++;
  label->next = current_fn->labels;
  current_fn->labels = label;
  Node *node = new_node(ND_LABEL);
  node->label = label;
  token = token->next;
  node->endline = TRUE;
  return node;
}

Node *if_stmt() {
  token = token->next;
  Node *node = new_node(ND_IF);
  node->id = loop_cnt++;
  expect("(", "before condition", "if");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "if");
  node->then = stmt();
  if (token->kind == TK_ELSE) {
    token = token->next;
    node->els = stmt();
  } else {
    node->els = NULL;
  }
  return node;
}

Node *while_stmt() {
  token = token->next;
  expect("(", "before condition", "while");
  Node *node = new_node(ND_WHILE);
  node->id = loop_cnt++;
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "while");
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  return node;
}

Node *do_while_stmt() {
  token = token->next;
  Node *node = new_node(ND_DOWHILE);
  node->id = loop_cnt++;
  node->then = stmt();
  if (token->kind != TK_WHILE) {
    error_at(token->str, "expected 'while' but got \"%.*s\" [in do-while statement]", token->len, token->str);
  }
  token = token->next;
  expect("(", "before condition", "do-while");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "do-while");
  int loop_id_prev = loop_id;
  loop_id = node->id;
  loop_id = loop_id_prev;
  expect(";", "after line", "do-while");
  node->endline = TRUE;
  return node;
}

Node *for_stmt() {
  int init;
  token = token->next;
  expect("(", "before initialization", "for");
  Node *node = new_node(ND_FOR);
  node->id = loop_cnt++;
  if (consume(";")) {
    node->init = new_node(ND_NONE);
    node->init->endline = TRUE;
    init = FALSE;
  } else if (is_type(token)) {
    Type *type = consume_type();
    Token *tok = expect_ident("variable declaration");
    node->init = local_variable_declaration(tok, type, FALSE);
    expect(";", "after initialization", "for");
    node->init->endline = TRUE;
    init = TRUE;
  } else {
    node->init = expr();
    expect(";", "after initialization", "for");
    node->init->endline = TRUE;
    init = FALSE;
  }
  if (consume(";")) {
    node->cond = new_num(1);
    node->cond->endline = FALSE;
  } else {
    node->cond = expr();
    node->cond->endline = FALSE;
    expect(";", "after condition", "for");
  }
  if (consume(")")) {
    node->step = new_node(ND_NONE);
    node->step->endline = TRUE;
  } else {
    node->step = expr();
    node->step->endline = TRUE;
    expect(")", "after step expression", "for");
  }
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  if (init) {
    current_fn->locals = current_fn->locals->next;
  }
  loop_id = loop_id_prev;
  return node;
}

Node *break_stmt() {
  if (loop_id == -1) {
    error_at(token->str, "stray break statement [in break statement]");
  }
  token = token->next;
  expect(";", "after line", "break");
  Node *node = new_node(ND_BREAK);
  node->endline = TRUE;
  node->id = loop_id;
  return node;
}

Node *continue_stmt() {
  if (loop_id == -1) {
    error_at(token->str, "stray continue statement [in continue statement]");
  }
  token = token->next;
  expect(";", "after line", "continue");
  Node *node = new_node(ND_CONTINUE);
  node->endline = TRUE;
  node->id = loop_id;
  return node;
}

Node *return_stmt() {
  token = token->next;
  Node *node = new_node(ND_RETURN);
  if (consume(";")) {
    node->rhs = new_num(0);
  } else {
    node->rhs = expr();
    expect(";", "after line", "return");
  }
  node->endline = TRUE;
  return node;
}

Node *stmt() {
  Node *node;
  if (consume("{")) {
    node = block_stmt();
  } else if (token->kind == TK_GOTO) {
    node = goto_stmt();
    expect(";", "after line", "goto statement");
  } else if (token->kind == TK_LABEL) {
    node = label_stmt();
  } else if (token->kind == TK_EXTERN) {
    token = token->next;
    node = vardec_and_funcdef_stmt(FALSE, TRUE);
  } else if (token->kind == TK_STATIC) {
    token = token->next;
    node = vardec_and_funcdef_stmt(TRUE, FALSE);
  } else if (is_type(token)) {
    node = vardec_and_funcdef_stmt(FALSE, FALSE);
  } else if (token->kind == TK_STRUCT) {
    node = struct_stmt();
  } else if (token->kind == TK_TYPEDEF) {
    node = typedef_stmt();
  } else if (token->kind == TK_IF) {
    node = if_stmt();
  } else if (token->kind == TK_WHILE) {
    node = while_stmt();
  } else if (token->kind == TK_DO) {
    node = do_while_stmt();
  } else if (token->kind == TK_FOR) {
    node = for_stmt();
  } else if (token->kind == TK_BREAK) {
    node = break_stmt();
  } else if (token->kind == TK_CONTINUE) {
    node = continue_stmt();
  } else if (token->kind == TK_RETURN) {
    node = return_stmt();
  } else {
    node = expr();
    expect(";", "after line", "expression");
    node->endline = TRUE;
  }
  return node;
}
