#include "lacc.h"

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

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
  int cond = FALSE;
  if (!ignoring)
    cond = find_macro(name_start, name_len) != NULL;

  IfState *state = malloc(sizeof(IfState));
  if (!state)
    error("memory allocation failed");
  state->prev = if_stack;
  state->start = hash;
  state->branch_taken = FALSE;
  state->currently_active = FALSE;
  state->ignoring = ignoring;
  state->else_seen = FALSE;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = TRUE;
    state->currently_active = TRUE;
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
  int cond = FALSE;
  if (!ignoring) {
    int macro_defined = find_macro(name_start, name_len) != NULL;
    cond = !macro_defined;
  }

  IfState *state = malloc(sizeof(IfState));
  if (!state)
    error("memory allocation failed");
  state->prev = if_stack;
  state->start = hash;
  state->branch_taken = FALSE;
  state->currently_active = FALSE;
  state->ignoring = ignoring;
  state->else_seen = FALSE;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = TRUE;
    state->currently_active = TRUE;
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
  int cond = FALSE;
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
  state->branch_taken = FALSE;
  state->currently_active = FALSE;
  state->ignoring = ignoring;
  state->else_seen = FALSE;
  if_stack = state;

  if (state->ignoring || !cond) {
    skip_depth++;
  } else {
    state->branch_taken = TRUE;
    state->currently_active = TRUE;
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
  int cond = FALSE;
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
      state->currently_active = FALSE;
      skip_depth++;
    }
    if (!state->branch_taken && cond) {
      state->branch_taken = TRUE;
      state->currently_active = TRUE;
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

  state->else_seen = TRUE;
  if (!state->ignoring) {
    if (state->currently_active) {
      state->currently_active = FALSE;
      skip_depth++;
    }
    if (!state->branch_taken) {
      state->branch_taken = TRUE;
      state->currently_active = TRUE;
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
