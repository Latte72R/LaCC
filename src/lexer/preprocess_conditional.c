#include "diagnostics.h"
#include "lexer.h"

#include "lexer_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct IfState IfState;
struct IfState {
  IfState *prev;
  char *start;
  int branch_taken;
  int currently_active;
  int ignoring;
  int else_seen;
};

static IfState *if_stack = 0;
static int skip_depth = 0;

int preprocess_is_skipping() { return skip_depth > 0; }

static char *read_directive_message(char *body_start, char **out_next) {
  char *p = skip_directive_spaces(body_start);
  char *line_end = find_directive_line_end(body_start);

  int cap = 64;
  int len = 0;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");

  while (p < line_end) {
    if (*p == '\\') {
      if (p + 1 < line_end && p[1] == '\n') {
        p += 2;
        continue;
      }
      if (p + 2 < line_end && p[1] == '\r' && p[2] == '\n') {
        p += 3;
        continue;
      }
    }
    if (*p == '\n')
      break;
    if (len + 1 >= cap) {
      cap *= 2;
      buf = realloc(buf, cap);
      if (!buf)
        error("memory allocation failed");
    }
    buf[len++] = *p++;
  }

  char *msg = copy_trim(buf, buf + len);
  free(buf);
  if (*line_end == '\n')
    line_end++;
  *out_next = line_end;
  return msg;
}

void preprocess_check_unterminated_ifs() {
  if (if_stack) {
    error_at(new_location(if_stack->start), "unterminated #if directive");
  }
}

int parse_ifdef_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "ifdef"))
    return 0;

  char *name_start = cur;
  if (!is_ident_start_char(*name_start))
    error_at(new_location(name_start), "expected identifier after #ifdef");

  char *name_end = name_start;
  while (is_ident_char(*name_end))
    name_end++;
  int name_len = (int)(name_end - name_start);

  char *rest = skip_trailing_spaces_and_comments(name_end);
  if (*rest && *rest != '\n')
    error_at(new_location(rest), "unexpected tokens after identifier in #ifdef");

  int ignoring = skip_depth > 0;
  int cond = false;
  if (!ignoring)
    cond = find_macro(name_start, name_len) != NULL;

  IfState *state = malloc(sizeof(IfState));
  if (!state)
    error("memory allocation failed");
  state->prev = if_stack;
  state->start = hash;
  state->branch_taken = false;
  state->currently_active = false;
  state->ignoring = ignoring;
  state->else_seen = false;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = true;
    state->currently_active = true;
  }

  if (*rest == '\n')
    rest++;
  *p = rest;
  return 1;
}

int parse_ifndef_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "ifndef"))
    return 0;

  char *name_start = cur;
  if (!is_ident_start_char(*name_start))
    error_at(new_location(name_start), "expected identifier after #ifndef");

  char *name_end = name_start;
  while (is_ident_char(*name_end))
    name_end++;
  int name_len = (int)(name_end - name_start);

  char *rest = skip_trailing_spaces_and_comments(name_end);
  if (*rest && *rest != '\n')
    error_at(new_location(rest), "unexpected tokens after identifier in #ifndef");

  int ignoring = skip_depth > 0;
  int cond = false;
  if (!ignoring) {
    int macro_defined = find_macro(name_start, name_len) != NULL;
    cond = !macro_defined;
  }

  IfState *state = malloc(sizeof(IfState));
  if (!state)
    error("memory allocation failed");
  state->prev = if_stack;
  state->start = hash;
  state->branch_taken = false;
  state->currently_active = false;
  state->ignoring = ignoring;
  state->else_seen = false;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = true;
    state->currently_active = true;
  }

  if (*rest == '\n')
    rest++;
  *p = rest;
  return 1;
}

int parse_if_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "if"))
    return 0;

  char *expr_start = cur;
  char *scan = find_directive_line_end(expr_start);

  int ignoring = skip_depth > 0;
  int cond = false;
  if (ignoring) {
    char *trimmed = copy_trim_directive_expr(expr_start, scan);
    if (!trimmed[0]) {
      free(trimmed);
      error_at(new_location(expr_start), "expected expression after #if");
    }
    free(trimmed);
  } else if (!preprocess_evaluate_if_expression(expr_start, scan, &cond)) {
    error_at(new_location(expr_start), "invalid expression in #if directive");
  }

  IfState *state = malloc(sizeof(IfState));
  if (!state)
    error("memory allocation failed");
  state->prev = if_stack;
  state->start = hash;
  state->branch_taken = false;
  state->currently_active = false;
  state->ignoring = ignoring;
  state->else_seen = false;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = true;
    state->currently_active = true;
  }

  if (*scan == '\n')
    scan++;
  *p = scan;
  return 1;
}

int parse_elif_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "elif"))
    return 0;

  if (!if_stack)
    error_at(new_location(hash), "#elif without matching #if");
  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(hash), "#elif after #else");

  char *expr_start = cur;
  char *scan = find_directive_line_end(expr_start);

  int should_eval = !state->ignoring && !state->branch_taken;
  int cond = false;
  if (should_eval && !preprocess_evaluate_if_expression(expr_start, scan, &cond)) {
    error_at(new_location(expr_start), "invalid expression in #elif directive");
  } else if (!should_eval) {
    char *trimmed = copy_trim_directive_expr(expr_start, scan);
    if (!trimmed[0]) {
      free(trimmed);
      error_at(new_location(expr_start), "expected expression after #elif");
    }
    free(trimmed);
  }

  if (!state->ignoring) {
    if (state->currently_active) {
      state->currently_active = false;
      skip_depth++;
    }
    if (!state->branch_taken && cond) {
      state->branch_taken = true;
      state->currently_active = true;
      if (skip_depth <= 0)
        error_at(new_location(hash), "internal error: skip depth underflow");
      skip_depth--;
    }
  }

  if (*scan == '\n')
    scan++;
  *p = scan;
  return 1;
}

int parse_else_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "else"))
    return 0;

  if (!if_stack)
    error_at(new_location(hash), "#else without matching #if");
  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(hash), "multiple #else directives");

  char *check = skip_trailing_spaces_and_comments(cur);
  if (*check && *check != '\n')
    error_at(new_location(check), "unexpected tokens after #else");

  state->else_seen = true;
  if (!state->ignoring) {
    if (state->currently_active) {
      state->currently_active = false;
      skip_depth++;
    }
    if (!state->branch_taken) {
      state->branch_taken = true;
      state->currently_active = true;
      if (skip_depth <= 0)
        error_at(new_location(hash), "internal error: skip depth underflow");
      skip_depth--;
    }
  }

  while (*check && *check != '\n')
    check++;
  if (*check == '\n')
    check++;
  *p = check;
  return 1;
}

int parse_endif_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (!consume_directive_keyword(&cur, "endif"))
    return 0;

  if (!if_stack)
    error_at(new_location(hash), "#endif without matching #if");
  IfState *state = if_stack;
  if_stack = state->prev;

  if (state->ignoring || !state->currently_active) {
    if (skip_depth <= 0)
      error_at(new_location(hash), "internal error: skip depth underflow");
    skip_depth--;
  }
  free(state);

  char *check = skip_trailing_spaces_and_comments(cur);
  if (*check && *check != '\n')
    error_at(new_location(check), "unexpected tokens after #endif");
  while (*check && *check != '\n')
    check++;
  if (*check == '\n')
    check++;
  *p = check;
  return 1;
}

int parse_error_directive(char **p) {
  char *cur = *p;
  if (!consume_directive_keyword(&cur, "error"))
    return 0;

  char *next = cur;
  char *msg = read_directive_message(cur, &next);
  if (!preprocess_is_skipping()) {
    if (msg[0])
      error_at(new_location(cur), "%s", msg);
    else
      error_at(new_location(cur), "#error");
  }
  free(msg);
  *p = next;
  return 1;
}

int parse_warning_directive(char **p) {
  char *cur = *p;
  if (!consume_directive_keyword(&cur, "warning"))
    return 0;

  char *next = cur;
  char *msg = read_directive_message(cur, &next);

  if (!preprocess_is_skipping()) {
    if (msg[0])
      warning_at(new_location(cur), "#warning %s", msg);
    else
      warning_at(new_location(cur), "#warning");
  }

  free(msg);
  *p = next;
  return 1;
}

int parse_pragma_directive(char **p) {
  char *cur = *p;
  if (!consume_directive_keyword(&cur, "pragma"))
    return 0;

  // Ignore pragmas we don't understand; just consume to end of line (handling splices for consistency)
  char *next = cur;
  char *msg = read_directive_message(cur, &next);
  free(msg);
  *p = next;
  return 1;
}
