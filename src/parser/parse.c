
#include "lacc.h"

extern Token *token;
extern Node **code;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

void error_duplicate_name(Token *tok, const char *type) {
  error_at(tok->str, "duplicated %s name: %.*s", type, tok->len, tok->str);
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
  code[i] = new_node(ND_NONE);
}
