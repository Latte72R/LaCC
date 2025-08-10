
#include "lacc.h"

extern Token *token;
extern int label_cnt;
extern int loop_cnt;
extern int loop_id;
extern int block_cnt;
extern int block_id;
extern Function *current_fn;
extern LVar *locals;
extern Node *current_switch;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern ObjectTag *object_tags;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Node *block_stmt() {
  Node *node = new_node(ND_BLOCK);
  LVar *locals_prev = locals;
  Object *structs_prev = structs;
  Object *unions_prev = unions;
  Object *enums_prev = enums;
  ObjectTag *object_tags_prev = object_tags;
  int block_id_prev = block_id;
  block_id = block_cnt++;
  node->body = malloc(sizeof(Node *));
  int i = 0;
  while (!consume("}")) {
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1);
    node->body[i++] = stmt();
  }
  node->body[i] = new_node(ND_NONE);
  block_id = block_id_prev;
  locals = locals_prev;
  structs = structs_prev;
  unions = unions_prev;
  enums = enums_prev;
  object_tags = object_tags_prev;
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
  expect(";", "after line", "goto statement");
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
  expect(":", "after label", "label statement");
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
  int loop_id_prev = loop_id;
  node->id = loop_cnt++;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  if (token->kind != TK_WHILE) {
    error_at(token->str, "expected 'while' after do-while statement [in do-while]");
  }
  token = token->next;
  expect("(", "before condition", "do-while");
  node->cond = expr();
  node->cond->endline = FALSE;
  expect(")", "after equality", "do-while");
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
    Type *type = consume_type(TRUE);
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
    locals = locals->next;
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
  char *ptr = token->str;
  Node *node = new_node(ND_RETURN);
  if (consume(";")) {
    node->rhs = new_num(0);
  } else {
    if (current_fn->type->ty == TY_VOID) {
      warning_at(ptr, "returning value from void function [in return statement]");
    }
    node->rhs = expr();
    expect(";", "after line", "return");
  }
  if (current_fn->type->ty != TY_VOID && !is_same_type(current_fn->type, node->rhs->type)) {
    warning_at(ptr, "incompatible %s to %s conversion [in return statement]", type_name(node->rhs->type),
               type_name(current_fn->type));
  }
  node->endline = TRUE;
  return node;
}

Node *switch_stmt() {
  token = token->next;
  expect("(", "before condition", "switch");
  Node *node = new_node(ND_SWITCH);
  node->id = loop_cnt++;
  node->cond = expr();
  node->cond->endline = FALSE;
  node->cases = malloc(sizeof(int *));
  node->case_cnt = 0;
  node->has_default = FALSE;
  expect(")", "after condition", "switch");
  Node *prev_switch = current_switch;
  current_switch = node;
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  current_switch = prev_switch;
  node->endline = TRUE;
  return node;
}

Node *case_stmt() {
  if (!current_switch) {
    error_at(token->str, "stray case statement [in case statement]");
  }
  token = token->next;
  Node *node = new_node(ND_CASE);
  node->id = loop_id;
  node->val = compile_time_number();
  for (int i = 0; i < current_switch->case_cnt; i++) {
    if (current_switch->cases[i] == node->val) {
      error_at(token->str, "duplicate case value [in case statement]");
    }
  }
  current_switch->cases[current_switch->case_cnt] = node->val;
  current_switch->cases = safe_realloc_array(current_switch->cases, sizeof(int *), ++current_switch->case_cnt);
  expect(":", "after case value", "case");
  node->endline = TRUE;
  return node;
}

Node *default_stmt() {
  if (!current_switch) {
    error_at(token->str, "stray default statement [in default statement]");
  } else if (current_switch->has_default) {
    error_at(token->str, "multiple default statements in switch [in default statement]");
  }
  token = token->next;
  current_switch->has_default = TRUE;
  Node *node = new_node(ND_DEFAULT);
  node->id = loop_id;
  expect(":", "after default", "default");
  node->endline = TRUE;
  return node;
}

int check_label() {
  Token *tok = token;
  if (tok->kind != TK_IDENT) {
    return FALSE;
  }
  tok = tok->next;
  if (tok->kind != TK_RESERVED || strncmp(tok->str, ":", tok->len)) {
    return FALSE;
  }
  return TRUE;
}

Node *expression_stmt() {
  Node *node = expr();
  node->endline = TRUE;
  expect(";", "after line", "expression");
  return node;
}

Node *stmt() {
  Node *node;
  if (consume("{")) {
    node = block_stmt();
  } else if (token->kind == TK_GOTO) {
    node = goto_stmt();
  } else if (check_label()) {
    node = label_stmt();
  } else if (token->kind == TK_EXTERN) {
    token = token->next;
    node = vardec_and_funcdef_stmt(FALSE, TRUE);
  } else if (token->kind == TK_STATIC) {
    token = token->next;
    node = vardec_and_funcdef_stmt(TRUE, FALSE);
  } else if (is_type(token)) {
    node = vardec_and_funcdef_stmt(FALSE, FALSE);
  } else if (token->kind == TK_TYPEDEF) {
    node = typedef_stmt();
  } else if (token->kind == TK_SWITCH) {
    node = switch_stmt();
  } else if (token->kind == TK_CASE) {
    node = case_stmt();
  } else if (token->kind == TK_DEFAULT) {
    node = default_stmt();
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
    node = expression_stmt();
  }
  return node;
}
