
#include "lacc.h"

#include <stdlib.h>
#include <string.h>

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

static InputContext *current_input_context = 0;

void push_input_context(char *input, char *path, int line_offset) {
  InputContext *ctx = malloc(sizeof(InputContext));
  if (!ctx)
    error("memory allocation failed");
  ctx->input = input;
  ctx->path = path;
  ctx->line_offset = line_offset;
  ctx->prev = current_input_context;
  current_input_context = ctx;
}

void pop_input_context(void) {
  if (!current_input_context)
    return;
  InputContext *prev = current_input_context->prev;
  free(current_input_context);
  current_input_context = prev;
}

static void compute_line_info(InputContext *ctx, char *loc, const char **out_line_start, const char **out_line_end,
                              int *out_line, int *out_column) {
  const char *input_start = ctx ? ctx->input : user_input;
  const char *line = loc;
  while (input_start && line > input_start && line[-1] != '\n')
    line--;

  const char *end = loc;
  while (*end != '\n' && *end != '\0')
    end++;

  int line_num = ctx ? ctx->line_offset + 1 : 1;
  if (input_start) {
    for (const char *p = input_start; p < line; p++)
      if (*p == '\n')
        line_num++;
  }

  int column = (int)(loc - line);
  *out_line_start = line;
  *out_line_end = end;
  *out_line = line_num;
  *out_column = column;
}

int get_line_number(char *pos) {
  InputContext *ctx = current_input_context;
  if (!ctx || !ctx->input || !pos)
    return 1;
  int line = ctx->line_offset + 1;
  for (char *p = ctx->input; p < pos && *p; p++)
    if (*p == '\n')
      line++;
  return line;
}

Location *new_location(char *loc) {
  Location *location = malloc(sizeof(Location));
  if (!location)
    error("memory allocation failed");
  location->loc = loc;
  InputContext *ctx = current_input_context;
  location->path = ctx ? ctx->path : input_file;
  location->input = ctx ? ctx->input : user_input;
  if (!loc || !location->input) {
    location->line_start = NULL;
    location->line_end = NULL;
    location->line = 0;
    location->column = 0;
  } else {
    const char *line_start = NULL;
    const char *line_end = NULL;
    int line_num = 0;
    int column = 0;
    compute_line_info(ctx, loc, &line_start, &line_end, &line_num, &column);
    location->line_start = line_start;
    location->line_end = line_end;
    location->line = line_num;
    location->column = column;
  }
  register_location(location);
  return location;
}

// Create a new token and add it as the next token of `cur`.
void new_token(TokenKind kind, char *loc, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  tok->val = 0;
  tok->lit_rank = 0;
  tok->lit_is_unsigned = 0;
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
