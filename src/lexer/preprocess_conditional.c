#include "lacc.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// from preprocess_table.c
extern Macro *find_macro(char *name, int len);

// helpers
extern char *skip_trailing_spaces_and_comments(char *cur);
extern int is_ident_start_char(char c);
extern int is_ident_char(char c);
extern char *copy_trim_directive_expr(const char *start, const char *end);

// evaluator
int preprocess_evaluate_if_expression(char *expr_start, char *expr_end, int *result);

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

int preprocess_is_skipping(void) { return skip_depth > 0; }

void preprocess_check_unterminated_ifs(void) {
  if (if_stack) {
    error_at(new_location(if_stack->start), "unterminated #if directive");
  }
}

int parse_ifdef_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "ifdef") && !is_alnum(cur[5])))
    return 0;

  char *name_start = cur + 5;
  while (*name_start == ' ' || *name_start == '\t')
    name_start++;
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
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "ifndef") && !is_alnum(cur[6])))
    return 0;

  char *name_start = cur + 6;
  while (*name_start == ' ' || *name_start == '\t')
    name_start++;
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
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "if") && !is_alnum(cur[2])))
    return 0;

  char *expr_start = cur + 2;
  while (*expr_start == ' ' || *expr_start == '\t')
    expr_start++;
  char *scan = expr_start;
  while (*scan) {
    if (*scan == '\n') {
      char *prev = scan - 1;
      while (prev >= expr_start && *prev == '\r')
        prev--;
      if (prev >= expr_start && *prev == '\\') {
        scan++;
        continue;
      }
      break;
    }
    scan++;
  }

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
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "elif") && !is_alnum(cur[4])))
    return 0;

  if (!if_stack)
    error_at(new_location(hash), "#elif without matching #if");
  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(hash), "#elif after #else");

  char *expr_start = cur + 4;
  while (*expr_start == ' ' || *expr_start == '\t')
    expr_start++;
  char *scan = expr_start;
  while (*scan) {
    if (*scan == '\n') {
      char *prev = scan - 1;
      while (prev >= expr_start && *prev == '\r')
        prev--;
      if (prev >= expr_start && *prev == '\\') {
        scan++;
        continue;
      }
      break;
    }
    scan++;
  }

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
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "else") && !is_alnum(cur[4])))
    return 0;

  if (!if_stack)
    error_at(new_location(hash), "#else without matching #if");
  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(hash), "multiple #else directives");

  char *extra_start = cur + 4;
  char *check = skip_trailing_spaces_and_comments(extra_start);
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
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "endif") && !is_alnum(cur[5])))
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

  char *extra_start = cur + 5;
  char *check = skip_trailing_spaces_and_comments(extra_start);
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
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "error") && !is_alnum(cur[5])))
    return 0;

  // If currently skipping due to inactive branch, ignore the line (preprocess_is_skipping handles most cases)
  // but for consistency, just consume the rest of the line.
  char *msg_start = cur + 5;
  // Trim leading spaces/comments in the message
  char *scan = msg_start;
  // Skip leading spaces
  while (*scan == ' ' || *scan == '\t')
    scan++;
  // Copy message up to end of line
  char *line_end = scan;
  while (*line_end && *line_end != '\n')
    line_end++;
  // Trim leading/trailing whitespace from message
  char *front = scan;
  while (front < line_end && isspace((unsigned char)*front))
    front++;
  char *back = line_end;
  while (back > front && isspace((unsigned char)*(back - 1)))
    back--;
  int mlen = (int)(back - front);
  char *msg = malloc(mlen + 1);
  if (!msg)
    error("memory allocation failed");
  if (mlen > 0)
    memcpy(msg, front, mlen);
  msg[mlen] = '\0';
  if (!preprocess_is_skipping()) {
    if (msg[0])
      error_at(new_location(scan), "%s", msg);
    else
      error_at(new_location(scan), "#error");
  }
  free(msg);
  if (*line_end == '\n')
    line_end++;
  *p = line_end;
  return 1;
}

int parse_warning_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "warning") && !is_alnum(cur[7])))
    return 0;

  // Consume rest of the directive as the message, supporting line splices (\\\n)
  char *msg_start = cur + 7;
  char *scan = msg_start;
  while (*scan == ' ' || *scan == '\t')
    scan++;

  // Build message buffer across backslash-newline splices until an unspliced newline
  int cap = 64;
  int len = 0;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");
  char *q = scan;
  for (;;) {
    if (!*q)
      break;
    if (*q == '\\') {
      if (q[1] == '\n') {
        q += 2; // swallow backslash-newline
        continue;
      }
      if (q[1] == '\r' && q[2] == '\n') {
        q += 3; // swallow backslash-CRLF
        continue;
      }
    }
    if (*q == '\n')
      break;
    if (len + 1 >= cap) {
      cap *= 2;
      char *nb = realloc(buf, cap);
      if (!nb)
        error("memory allocation failed");
      buf = nb;
    }
    buf[len++] = *q++;
  }
  // Trim leading/trailing spaces of the built message
  int front = 0;
  while (front < len && isspace((unsigned char)buf[front]))
    front++;
  int back = len;
  while (back > front && isspace((unsigned char)buf[back - 1]))
    back--;
  int mlen = back - front;
  char *msg = malloc(mlen + 1);
  if (!msg)
    error("memory allocation failed");
  if (mlen > 0)
    memcpy(msg, buf + front, mlen);
  msg[mlen] = '\0';

  if (!preprocess_is_skipping()) {
    if (msg[0])
      warning_at(new_location(scan), "#warning %s", msg);
    else
      warning_at(new_location(scan), "#warning");
  }

  free(buf);
  free(msg);
  if (*q == '\n')
    q++;
  *p = q;
  return 1;
}

int parse_pragma_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "pragma") && !is_alnum(cur[6])))
    return 0;

  // Ignore pragmas we don't understand; just consume to end of line
  char *scan = cur + 6;
  while (*scan && *scan != '\n')
    scan++;
  if (*scan == '\n')
    scan++;
  *p = scan;
  return 1;
}
