
#include "lacc.h"

extern Token *token;
extern char *consumed_ptr;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

int peek(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return FALSE;
  return TRUE;
}

int consume(char *op) {
  if (!peek(op))
    return FALSE;
  consumed_ptr = token->str;
  token = token->next;
  return TRUE;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  consumed_ptr = token->str;
  token = token->next;
  return tok;
}

// Ensure that the current token is `op`.
void expect(char *op, char *err, char *stmt) {
  if (strncmp(token->str, op, token->len))
    error_at(token->str, "expected \"%s\":\n  %s  [in %s statement]", op, err, stmt);
  token = token->next;
}

// Ensure that the current token is `op`.
void error_expected_at(char *loc, char *op, char *err, char *stmt) {
  error_at(loc, "expected \"%s\" but got \"%s\" [in %s statement]", op, err, stmt);
}

Token *expect_ident(char *stmt) {
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier [in %s statement]", stmt);
  }
  return tok;
}

// Ensure that the current token is TK_NUM.
int expect_number(char *stmt) {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number [in %s statement]", stmt);
  int val = token->val;
  token = token->next;
  return val;
}

int parse_sign() {
  if (consume("-"))
    return -1;
  if (consume("+"))
    return 1;
  return 1;
}

// 数値の期待値取得（符号付き）
int expect_signed_number() {
  int sign = parse_sign();
  return expect_number("expect signed number") * sign;
}
