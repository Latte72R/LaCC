
#include "lacc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern Token *token;
extern Node **code;
extern Location *consumed_loc;
extern int label_cnt;
extern int array_cnt;
extern String *strings;
extern Array *arrays;

int peek(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || strncmp(token->str, op, token->len))
    return false;
  return true;
}

int consume(char *op) {
  if (!peek(op))
    return false;
  consumed_loc = token->loc;
  token = token->next;
  return true;
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
  // トークン切れや不正状態でのセグフォ回避と親切なエラーメッセージ
  if (!token || token->kind == TK_EOF) {
    Location *loc = consumed_loc ? consumed_loc : (token ? token->loc : NULL);
    if (loc)
      error_at(loc, "unexpected end of input: expected \"%s\" %s [in %s]", op, err, stmt);
    error("unexpected end of input while expecting '%s'", op);
  }
  if (token->kind != TK_RESERVED || strlen(op) != token->len || strncmp(token->str, op, token->len))
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

void *safe_realloc_array(void *ptr, int elem_size, int need, int *cap) {
  int old_cap = *cap;
  *cap = (*cap > 0) ? *cap : 8;
  while (need > *cap) {
    *cap = *cap * 2;
  }
  if (old_cap != *cap) {
    void *new_ptr = realloc(ptr, elem_size * *cap);
    if (!new_ptr)
      error("realloc failed");
    ptr = new_ptr;
  }
  return ptr;
}

void program() {
  int i = 0;
  int cap = 16;
  code = malloc(sizeof(Node *) * cap);
  while (token->kind != TK_EOF) {
    code = safe_realloc_array(code, sizeof(Node *), i + 1, &cap);
    code[i++] = stmt();
  }
  code = safe_realloc_array(code, sizeof(Node *), i + 1, &cap);
  code[i] = new_node(ND_NONE);
}

static Type *array_base_type(Type *type) {
  while (type && type->ty == TY_ARR)
    type = type->ptr_to;
  return type;
}

static void append_array_value(Array *array, int value, String *str, int *idx) {
  array->val = safe_realloc_array(array->val, sizeof(int), *idx + 1, &array->val_cap);
  array->str = safe_realloc_array(array->str, sizeof(String *), *idx + 1, &array->str_cap);
  array->val[*idx] = value;
  array->str[*idx] = str;
  (*idx)++;
}

static int parse_array_initializer_value(Array *array, Type *type, int *idx) {
  if (type->ty == TY_ARR) {
    if (peek("{")) {
      consume("{");
      int provided = 0;
      while (!peek("}")) {
        parse_array_initializer_value(array, type->ptr_to, idx);
        provided++;
        if (!consume(","))
          break;
        if (peek("}"))
          break;
      }
      expect("}", "after array initializer", "array_literal");
      if (type->array_size == 0)
        type->array_size = provided;
      else if (provided > type->array_size)
        warning_at(consumed_loc, "excess elements in array initializer [in variable declaration]");
      return 1;
    }
    if (type->ptr_to->ty == TY_CHAR && token->kind == TK_STRING) {
      Token *tok = token;
      consumed_loc = tok->loc;
      token = token->next;
      int declared = type->array_size;
      int copy_len = tok->len;
      if (declared == 0) {
        declared = copy_len;
        type->array_size = declared;
      } else if (copy_len > declared) {
        warning_at(tok->loc, "initializer-string for char array is too long [in variable declaration]");
        copy_len = declared;
      }
      for (int i = 0; i < declared; i++) {
        int value = 0;
        if (i < copy_len)
          value = (unsigned char)tok->str[i];
        append_array_value(array, value, NULL, idx);
      }
      return 1;
    }
    parse_array_initializer_value(array, type->ptr_to, idx);
    if (type->array_size == 0)
      type->array_size = 1;
    return 1;
  }

  if (type->ty == TY_PTR && token->kind == TK_STRING) {
    Token *tok = token;
    consumed_loc = tok->loc;
    String *str = string_literal();
    append_array_value(array, 0, str, idx);
    return 1;
  }

  int sign = parse_sign();
  Token *tok = token; // capture location of the numeric token after optional sign
  int value = expect_number("array_literal");
  value *= sign;
  consumed_loc = tok->loc;
  // For _Bool elements, normalize initializer to 0 or 1 per C rules
  if (type->ty == TY_BOOL) {
    value = (value != 0);
  }
  append_array_value(array, value, NULL, idx);
  return 1;
}

Array *array_literal(Type *type) {
  Type *org_type = type->ptr_to;
  expect("{", "before array initializer", "array_literal");
  Array *array = malloc(sizeof(Array));
  array->id = array_cnt++;
  Type *base = array_base_type(org_type);
  array->byte = get_sizeof(base);
  array->val_cap = 16;
  array->str_cap = 16;
  int idx = 0;
  array->val = malloc(sizeof(int) * array->val_cap);
  array->str = calloc(array->str_cap, sizeof(String *));
  array->next = arrays;
  array->elem_count = 0;
  arrays = array;
  while (!peek("}")) {
    parse_array_initializer_value(array, org_type, &idx);
    array->elem_count++;
    if (!consume(","))
      break;
    if (peek("}"))
      break;
  }
  expect("}", "after array initializer", "array_literal");
  array->len = idx;
  array->init = idx;
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
  str->next = strings;
  strings = str;
  token = token->next;
  return str;
}
