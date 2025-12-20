#include "diagnostics.h"
#include "lexer.h"
#include "runtime.h"
#include "source.h"

#include "lexer_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void handle_include_common(char *name, char *p, bool is_system_header, bool is_next) {
  char *including_file = input_file;
  char *raw_name = name;
  char *resolved_path = NULL;
  char *new_input = NULL;
  if (is_next) {
    new_input = read_include_next_file(raw_name, including_file, &resolved_path);
  } else {
    new_input = read_include_file(raw_name, including_file, is_system_header, &resolved_path);
  }
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open %s file: %s", is_next ? "include_next" : "include", raw_name);
  }
  if (!resolved_path)
    resolved_path = duplicate_cstring(raw_name);
  free(raw_name);

  char *input_file_prev = input_file;
  input_file = resolved_path;
  int prev_level = hierarchy_level++;
  if (print_include_files) {
    for (int i = 0; i < hierarchy_level; i++)
      fprintf(stderr, ".");
    fprintf(stderr, " %s\n", resolved_path);
  }

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
  hierarchy_level = prev_level;
}

static void handle_include_directive(char *name, char *p, bool is_system_header) {
  handle_include_common(name, p, is_system_header, false);
}

static void handle_include_next_directive(char *name, char *p) { handle_include_common(name, p, false, true); }

int parse_include_directive(char **pcur) {
  char *cur = *pcur;
  int is_next;
  if (consume_directive_keyword(&cur, "include_next")) {
    is_next = 1;
  } else if (consume_directive_keyword(&cur, "include")) {
    is_next = 0;
  } else {
    return 0;
  }

  // Capture and preprocess the remainder of the directive (handles line splices)
  char *body_start = cur;
  char *scan = find_directive_line_end(body_start);

  char *trimmed = copy_trim_directive_expr(body_start, scan);
  if (!trimmed[0])
    error_at(new_location(body_start), "expected header name after #include");
  char *expanded = expand_expression_internal(trimmed);
  char *p = (char *)skip_spaces(expanded);

  char close;
  if (*p == '"') {
    close = '"';
  } else if (*p == '<') {
    close = '>';
  } else {
    free(trimmed);
    free(expanded);
    error_at(new_location(body_start), "expected '\"' or '<' after #include");
  }

  p++;
  char *name_start = p;
  while (*p && *p != close) {
    if (close == '"' && *p == '\\' && *(p + 1)) {
      p += 2;
      continue;
    }
    if (*p == '\n')
      error_at(new_location(body_start), "unexpected newline in #include directive");
    p++;
  }

  if (*p != close) {
    free(trimmed);
    free(expanded);
    error_at(new_location(body_start), "unterminated include filename");
  }

  size_t len = p - name_start;
  char *name = malloc(len + 1);
  if (!name)
    error("memory allocation failed");
  if (len > 0)
    memcpy(name, name_start, len);
  name[len] = '\0';

  p++; // skip closing delimiter
  p = (char *)skip_spaces(p);
  if (*p) {
    free(trimmed);
    free(expanded);
    error_at(new_location(body_start), "unexpected tokens after #include filename");
  }

  free(trimmed);
  free(expanded);

  if (is_next) {
    handle_include_next_directive(name, body_start);
  } else {
    int is_system_header = (close == '>');
    handle_include_directive(name, body_start, is_system_header);
  }

  char *rest = scan;
  if (*rest == '\n')
    rest++;
  *pcur = rest;
  return 1;
}
