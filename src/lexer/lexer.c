
#include "lacc.h"

extern char *user_input;
extern Token *token;
extern Token *token_head;
extern String *filenames;
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
  tok->str = str;
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

char *handle_include_directive(char *p) {
  char *q;
  p += 8;
  while (isspace(*p)) {
    p++;
  }
  if (*p != '"') {
    error_at(new_location(p), "expected \" before \"include\"");
  }
  p++;
  q = p;
  while (*p != '"') {
    if (*p == '\0') {
      error_at(new_location(q - 1), "unclosed string literal [in tokenize]");
    }
    if (*p == '\\') {
      p += 2;
    } else {
      p++;
    }
  }
  char *name = malloc(sizeof(char) * (p - q + 1));
  memcpy(name, q, p - q);
  name[p - q] = '\0';
  p++;
  int flag = 1;
  for (String *s = filenames; s; s = s->next) {
    if (!strncmp(s->text, name, strlen(name))) {
      flag = 0;
      break;
    }
  }
  if (flag == 0) {
    free(name);
    return p; // Already included
  }
  char *input_file_prev = input_file;
  input_file = name;
  filenames = malloc(sizeof(String));
  filenames->text = input_file;
  filenames->len = strlen(input_file);
  filenames->next = NULL;
  char *user_input_prev = user_input;
  char *new_input = find_file_includes(name);
  if (!new_input) {
    error_at(new_location(q - 1), "Cannot open include file: %s", name);
  }
  user_input = new_input;
  tokenize();
  user_input = user_input_prev;
  input_file = input_file_prev;
  return p; // Return the position after the closing quote
}
