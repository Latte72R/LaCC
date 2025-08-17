
#include "lacc.h"

extern Token *token;
extern Node **code;
extern Location *consumed_loc;
extern int label_cnt;
extern int array_cnt;
extern String *strings;
extern Array *arrays;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

int peek(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || strncmp(token->str, op, token->len))
    return FALSE;
  return TRUE;
}

int consume(char *op) {
  if (!peek(op))
    return FALSE;
  consumed_loc = token->loc;
  token = token->next;
  return TRUE;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  consumed_loc = token->loc;
  token = token->next;
  return tok;
}

// Ensure that the current token is `op`.
void expect(char *op, char *err, char *stmt) {
  if (strncmp(token->str, op, token->len))
    error_at(token->loc, "expected \"%s\" %s [in %s statement]", op, err, stmt);
  token = token->next;
}

Token *expect_ident(char *stmt) {
  Token *tok = consume_ident();
  if (!tok) {
    error_at(token->loc, "expected an identifier [in %s statement]", stmt);
  }
  return tok;
}

// Ensure that the current token is TK_NUM.
int expect_number(char *stmt) {
  if (token->kind != TK_NUM)
    error_at(token->loc, "expected a number [in %s statement]", stmt);
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

void error_duplicate_name(Token *tok, const char *type) {
  error_at(tok->loc, "duplicated %s name: %.*s", type, tok->len, tok->str);
}

void *safe_realloc_array(void *ptr, int element_size, int new_size) {
  void *new_ptr = realloc(ptr, element_size * new_size);
  if (!new_ptr)
    error("realloc failed");
  return new_ptr;
}

void program() {
  int i = 0;
  code = NULL;
  while (token->kind != TK_EOF) {
    code = safe_realloc_array(code, sizeof(Node *), i + 1);
    code[i++] = stmt();
  }
  code = safe_realloc_array(code, sizeof(Node *), i + 1);
  code[i] = new_node(ND_NONE);
}

Array *array_literal(Type *type) {
  Type *org_type = type->ptr_to;
  expect("{", "before array initializer", "array_literal");
  Array *array = malloc(sizeof(Array));
  array->id = array_cnt++;
  array->byte = get_sizeof(org_type);
  array->val = NULL;
  if (arrays)
    array->next = arrays;
  else
    array->next = NULL;
  arrays = array;
  int i = 0;
  do {
    array->val = safe_realloc_array(array->val, sizeof(int), i + 1);
    array->val[i++] = expect_number("array_literal");
  } while (consume(","));
  array->len = i;
  array->init = i;
  expect("}", "after array initializer", "array_literal");
  return array;
}

String *string_literal() {
  if (token->kind != TK_STRING) {
    error_at(token->loc, "expected a string literal [in string_literal]");
  }
  String *str = malloc(sizeof(String));
  str->text = token->str;
  str->len = token->len;
  str->id = label_cnt++;
  if (strings)
    str->next = strings;
  else
    str->next = NULL;
  strings = str;
  token = token->next;
  return str;
}
