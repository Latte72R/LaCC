#include "lacc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

// helpers
extern char *duplicate_cstring(const char *src);
extern char *skip_trailing_spaces_and_comments(char *cur);

// Read file API
char *read_include_file(char *name, const char *including_file, int is_system, char **resolved_name);
char *read_include_next_file(char *name, const char *including_file, char **resolved_name);

static void handle_include_directive(char *name, char *p, int is_system_header) {
  char *including_file = input_file;
  char *raw_name = name;
  char *resolved_path = NULL;
  char *new_input = read_include_file(raw_name, including_file, is_system_header, &resolved_path);
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open include file: %s", raw_name);
  }
  if (!resolved_path)
    resolved_path = duplicate_cstring(raw_name);
  free(raw_name);

  if (!resolved_path)
    resolved_path = duplicate_cstring("");

  for (FileName *s = filenames; s; s = s->next) {
    if (!strcmp(s->name, resolved_path)) {
      free(resolved_path);
      free(new_input);
      return;
    }
  }

  char *input_file_prev = input_file;
  input_file = resolved_path;
  FileName *filename = malloc(sizeof(FileName));
  filename->name = input_file;
  filename->next = filenames;
  filenames = filename;

  char *user_input_prev = user_input;
  CharPtrList *user_input_list_prev = user_input_list;
  user_input_list = malloc(sizeof(CharPtrList));
  if (!user_input_list)
    error("memory allocation failed");
  user_input_list->str = new_input;
  user_input_list->next = user_input_list_prev;

  user_input = new_input;
  push_input_context(new_input, input_file, 0);
  tokenize();
  pop_input_context();

  user_input = user_input_prev;
  input_file = input_file_prev;
}

static void handle_include_next_directive(char *name, char *p) {
  char *including_file = input_file;
  char *raw_name = name;
  char *resolved_path = NULL;
  char *new_input = read_include_next_file(raw_name, including_file, &resolved_path);
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open include_next file: %s", raw_name);
  }
  if (!resolved_path)
    resolved_path = duplicate_cstring(raw_name);
  free(raw_name);

  if (!resolved_path)
    resolved_path = duplicate_cstring("");

  for (FileName *s = filenames; s; s = s->next) {
    if (!strcmp(s->name, resolved_path)) {
      free(resolved_path);
      free(new_input);
      return;
    }
  }

  char *input_file_prev = input_file;
  input_file = resolved_path;
  FileName *filename = malloc(sizeof(FileName));
  filename->name = input_file;
  filename->next = filenames;
  filenames = filename;

  char *user_input_prev = user_input;
  CharPtrList *user_input_list_prev = user_input_list;
  user_input_list = malloc(sizeof(CharPtrList));
  if (!user_input_list)
    error("memory allocation failed");
  user_input_list->str = new_input;
  user_input_list->next = user_input_list_prev;

  user_input = new_input;
  push_input_context(new_input, input_file, 0);
  tokenize();
  pop_input_context();

  user_input = user_input_prev;
  input_file = input_file_prev;
}

int parse_include_directive(char **pcur) {
  char *cur = *pcur;
  if (*cur != '#') {
    return 0;
  }
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;

  int is_next = 0;
  if (startswith(cur, "include_next") && !is_alnum(cur[12])) {
    is_next = 1;
    cur += 12;
  } else if (startswith(cur, "include") && !is_alnum(cur[7])) {
    cur += 7;
  } else {
    return 0;
  }
  while (*cur == ' ' || *cur == '\t')
    cur++;

  char close;
  if (*cur == '"') {
    close = '"';
  } else if (*cur == '<') {
    close = '>';
  } else {
    error_at(new_location(cur), "expected '\"' or '<' after #include");
  }

  cur++;
  char *name_start = cur;
  while (*cur && *cur != close) {
    if (close == '"' && *cur == '\\') {
      cur++;
      if (!*cur)
        error_at(new_location(name_start), "unclosed string literal [in tokenize]");
      cur++;
      continue;
    }
    if (*cur == '\n')
      error_at(new_location(cur), "unexpected newline in #include directive");
    cur++;
  }

  if (*cur != close) {
    error_at(new_location(name_start - 1), "unterminated include filename");
  }

  int len = (int)(cur - name_start);
  char *name = malloc(len + 1);
  if (!name)
    error("memory allocation failed");
  if (len > 0)
    memcpy(name, name_start, len);
  name[len] = '\0';

  cur++; // skip closing delimiter
  char *rest = skip_trailing_spaces_and_comments(cur);
  if (*rest && *rest != '\n') {
    error_at(new_location(rest), "unexpected tokens after #include filename");
  }

  if (is_next) {
    handle_include_next_directive(name, name_start);
  } else {
    int is_system_header = (close == '>');
    handle_include_directive(name, name_start, is_system_header);
  }

  if (*rest == '\n')
    rest++;
  *pcur = rest;
  return 1;
}
