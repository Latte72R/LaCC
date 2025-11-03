
#include "lacc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern Token *token;
extern int label_cnt;
extern int label_cnt;
extern int loop_id;
extern int block_cnt;
extern int block_id;
extern Function *current_fn;
extern LVar *locals;
extern Node *current_switch;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern TypeTag *type_tags;
extern Location *consumed_loc;

static int token_equals(Token *tok, const char *str) {
  if (!tok)
    return false;
  if (tok->len != (int)strlen(str))
    return false;
  return strncmp(tok->str, str, tok->len) == 0;
}

static int is_inline_asm_keyword(Token *tok) {
  if (!tok)
    return false;
  if (tok->kind != TK_IDENT && tok->kind != TK_RESERVED)
    return false;
  return token_equals(tok, "__asm") || token_equals(tok, "__asm__");
}

Node *block_stmt() {
  Node *node = new_node(ND_BLOCK);
  LVar *locals_prev = locals;
  Object *structs_prev = structs;
  Object *unions_prev = unions;
  Object *enums_prev = enums;
  TypeTag *type_tags_prev = type_tags;
  int block_id_prev = block_id;
  block_id = block_cnt++;
  int cap = 16;
  node->body = malloc(sizeof(Node *) * cap);
  int i = 0;
  while (!consume("}")) {
    node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1, &cap);
    node->body[i++] = stmt();
  }
  node->body = safe_realloc_array(node->body, sizeof(Node *), i + 1, &cap);
  node->body[i] = new_node(ND_NONE);
  block_id = block_id_prev;
  locals = locals_prev;
  structs = structs_prev;
  unions = unions_prev;
  enums = enums_prev;
  // typedefで追加された型タグを解放
  while (type_tags != type_tags_prev) {
    TypeTag *next = type_tags->next;
    free(type_tags);
    type_tags = next;
  }
  type_tags = type_tags_prev;
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
  node->endline = true;
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
  node->endline = true;
  return node;
}

Node *if_stmt() {
  token = token->next;
  Node *node = new_node(ND_IF);
  node->id = label_cnt++;
  expect("(", "before condition", "if");
  node->cond = expr();
  node->cond->endline = false;
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
  node->id = label_cnt++;
  node->cond = expr();
  node->cond->endline = false;
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
  node->id = label_cnt++;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  if (token->kind != TK_WHILE) {
    error_at(token->loc, "expected 'while' after do-while statement [in do-while]");
  }
  token = token->next;
  expect("(", "before condition", "do-while");
  node->cond = expr();
  node->cond->endline = false;
  expect(")", "after equality", "do-while");
  expect(";", "after line", "do-while");
  node->endline = true;
  return node;
}

Node *for_stmt() {
  int has_decl_init = 0;
  LVar *locals_before_for = locals;
  token = token->next;
  expect("(", "before initialization", "for");
  Node *node = new_node(ND_FOR);
  node->id = label_cnt++;
  if (consume(";")) {
    node->init = new_node(ND_NONE);
    node->init->endline = true;
    has_decl_init = 0;
  } else if (is_type(token)) {
    // Declaration(s) in for-init: allow multiple declarators separated by commas
    // and keep their scope limited to this for-statement.
    Type *base_type = parse_base_type_internal(true, true);
    Node *blk = new_node(ND_BLOCK);
    int cap = 16;
    int i = 0;
    blk->body = malloc(sizeof(Node *) * cap);
    do {
      Token *tok;
      Type *type = parse_declarator(base_type, &tok, "variable declaration");
      if (!tok) {
        Location *loc = token ? token->loc : consumed_loc;
        error_at(loc, "expected an identifier [in variable declaration statement]");
      }
      blk->body = safe_realloc_array(blk->body, sizeof(Node *), i + 1, &cap);
      blk->body[i++] = local_variable_declaration(tok, type, false);
    } while (consume(","));
    blk->body = safe_realloc_array(blk->body, sizeof(Node *), i + 1, &cap);
    blk->body[i] = new_node(ND_NONE);
    expect(";", "after initialization", "for");
    blk->endline = true;
    node->init = blk;
    has_decl_init = 1;
  } else {
    node->init = expr();
    expect(";", "after initialization", "for");
    node->init->endline = true;
    has_decl_init = 0;
  }
  if (consume(";")) {
    node->cond = new_num(1);
    node->cond->endline = false;
  } else {
    node->cond = expr();
    node->cond->endline = false;
    expect(";", "after condition", "for");
  }
  if (consume(")")) {
    node->step = new_node(ND_NONE);
    node->step->endline = true;
  } else {
    node->step = expr();
    node->step->endline = true;
    expect(")", "after step expression", "for");
  }
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  // Limit the lifetime of for-init declarations to the for-statement
  if (has_decl_init) {
    locals = locals_before_for;
  }
  loop_id = loop_id_prev;
  return node;
}

Node *break_stmt() {
  if (loop_id == -1) {
    error_at(token->loc, "stray break statement [in break statement]");
  }
  token = token->next;
  expect(";", "after line", "break");
  Node *node = new_node(ND_BREAK);
  node->endline = true;
  node->id = loop_id;
  return node;
}

Node *continue_stmt() {
  if (loop_id == -1) {
    error_at(token->loc, "stray continue statement [in continue statement]");
  }
  token = token->next;
  expect(";", "after line", "continue");
  Node *node = new_node(ND_CONTINUE);
  node->endline = true;
  node->id = loop_id;
  return node;
}

Node *return_stmt() {
  token = token->next;
  Location *loc = token->loc;
  Node *node = new_node(ND_RETURN);
  if (consume(";")) {
    node->rhs = new_num(0);
  } else {
    if (current_fn->type->return_type->ty == TY_VOID) {
      warning_at(loc, "returning value from void function [in return statement]");
    }
    node->rhs = expr();
    expect(";", "after line", "return");
  }
  // 戻り値の期待型を保持しておく（コード生成での変換に利用）
  node->type = current_fn->type->return_type;
  if (current_fn->type->return_type->ty != TY_VOID &&
      !is_type_compatible(current_fn->type->return_type, node->rhs->type)) {
    warning_at(loc, "incompatible %s to %s conversion [in return statement]", type_name(node->rhs->type),
               type_name(current_fn->type->return_type));
  }
  node->endline = true;
  return node;
}

Node *switch_stmt() {
  token = token->next;
  expect("(", "before condition", "switch");
  Node *node = new_node(ND_SWITCH);
  node->id = label_cnt++;
  node->cond = expr();
  node->cond->endline = false;
  int cap = 16;
  node->cases = malloc(sizeof(int) * cap);
  node->case_cap = cap;
  node->case_cnt = 0;
  node->has_default = false;
  expect(")", "after condition", "switch");
  Node *prev_switch = current_switch;
  current_switch = node;
  int loop_id_prev = loop_id;
  loop_id = node->id;
  node->then = stmt();
  loop_id = loop_id_prev;
  current_switch = prev_switch;
  node->endline = true;
  return node;
}

Node *case_stmt() {
  if (!current_switch) {
    error_at(token->loc, "stray case statement [in case statement]");
  }
  token = token->next;
  Node *node = new_node(ND_CASE);
  node->id = loop_id;
  node->val = compile_time_number();
  for (int i = 0; i < current_switch->case_cnt; i++) {
    if (current_switch->cases[i] == node->val) {
      error_at(token->loc, "duplicate case value [in case statement]");
    }
  }
  current_switch->cases =
      safe_realloc_array(current_switch->cases, sizeof(int), current_switch->case_cnt + 1, &current_switch->case_cap);
  current_switch->cases[current_switch->case_cnt++] = node->val;
  expect(":", "after case value", "case");
  node->endline = true;
  return node;
}

Node *default_stmt() {
  if (!current_switch) {
    error_at(token->loc, "stray default statement [in default statement]");
  } else if (current_switch->has_default) {
    error_at(token->loc, "multiple default statements in switch [in default statement]");
  }
  token = token->next;
  current_switch->has_default = true;
  Node *node = new_node(ND_DEFAULT);
  node->id = loop_id;
  expect(":", "after default", "default");
  node->endline = true;
  return node;
}

int check_label() {
  Token *tok = token;
  if (tok->kind != TK_IDENT) {
    return false;
  }
  tok = tok->next;
  if (tok->kind != TK_RESERVED || strncmp(tok->str, ":", tok->len)) {
    return false;
  }
  return true;
}

Node *expression_stmt() {
  Node *node = expr();
  node->endline = true;
  expect(";", "after line", "expression");
  return node;
}

static void skip_parenthesized_sequence(Location *loc, const char *context) {
  int depth = 1;
  while (depth > 0) {
    if (!token || token->kind == TK_EOF) {
      error_at(loc, "unterminated parenthesized sequence [in %s]", context);
    }
    if (token->kind == TK_RESERVED && token->len == 1) {
      if (token->str[0] == '(')
        depth++;
      else if (token->str[0] == ')')
        depth--;
    }
    token = token->next;
  }
}

static Node *inline_asm_stmt() {
  Location *loc = token->loc;
  token = token->next;
  while (
      token && token->kind == TK_IDENT &&
      (token_equals(token, "__volatile__") || token_equals(token, "volatile") || token_equals(token, "__volatile"))) {
    token = token->next;
  }
  expect("(", "after inline assembly keyword", "asm statement");
  skip_parenthesized_sequence(loc, "asm statement");
  expect(";", "after inline assembly", "asm statement");
  Node *node = new_node(ND_ASM);
  node->endline = true;
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
    node = vardec_and_funcdef_stmt(false, true);
  } else if (token->kind == TK_STATIC) {
    token = token->next;
    node = vardec_and_funcdef_stmt(true, false);
  } else if (is_type(token)) {
    node = vardec_and_funcdef_stmt(false, false);
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
  } else if (is_inline_asm_keyword(token)) {
    node = inline_asm_stmt();
  } else {
    node = expression_stmt();
  }
  return node;
}
