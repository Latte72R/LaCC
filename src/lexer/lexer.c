
#include "lacc.h"

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Location *new_location(char *loc) {
  Location *location = malloc(sizeof(Location));
  location->loc = loc;
  location->path = input_file;
  location->input = user_input;
  return location;
}

// Create a new token and add it as the next token of `cur`.
void new_token(TokenKind kind, char *loc, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  if (len > 0) {
    tok->str = malloc(len + 1);
    strncpy(tok->str, str, len);
    tok->str[len] = '\0';
    register_char_ptr(tok->str);
  } else {
    tok->str = NULL;
  }
  tok->len = len;
  tok->next = NULL;
  tok->loc = new_location(loc);
  if (!token) {
    token_head = tok;
    token = tok;
  } else {
    token->next = tok;
    token = tok;
  }
}

int startswith(char *p, char *q) { return !strncmp(p, q, strlen(q)); }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}
