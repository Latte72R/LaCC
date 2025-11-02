#include "lacc.h"

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

// table
extern void define_macro(char *name, char *body, char **params, int param_count, int is_function, int is_variadic);
extern void undefine_macro(char *name, int len);

// helpers
extern int is_ident_start_char(char c);
extern int is_ident_char(char c);
extern char *skip_trailing_spaces_and_comments(char *cur);

int parse_define_directive(char **p) {
  char *cur = *p;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "define") && !is_alnum(cur[6]))) {
    return 0;
  }

  cur += 6;
  while (*cur == ' ' || *cur == '\t')
    cur++;

  if (!is_ident_start_char(*cur)) {
    error_at(new_location(cur), "expected an identifier after #define");
  }

  char *name_start = cur;
  while (is_ident_char(*cur))
    cur++;
  int name_len = (int)(cur - name_start);
  char *name = malloc(name_len + 1);
  if (!name)
    error("memory allocation failed");
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  int is_function = FALSE;
  int is_variadic = FALSE;
  char **params = NULL;
  int param_count = 0;
  int param_cap = 0;

  if (*cur == '(') {
    is_function = TRUE;
    cur++;
    if (*cur == ')') {
      cur++;
    } else {
      while (TRUE) {
        while (*cur == ' ' || *cur == '\t')
          cur++;
        // Variadic-only '...' or named variadic 'NAME...'
        if (*cur == '.' && cur[1] == '.' && cur[2] == '.') {
          // Unnamed variadic: use __VA_ARGS__ as the parameter name
          if (param_count >= param_cap) {
            param_cap = param_cap ? param_cap * 2 : 4;
            params = realloc(params, sizeof(char *) * param_cap);
            if (!params)
              error("memory allocation failed");
          }
          char *param_name = malloc(strlen("__VA_ARGS__") + 1);
          if (!param_name)
            error("memory allocation failed");
          memcpy(param_name, "__VA_ARGS__", strlen("__VA_ARGS__") + 1);
          params[param_count++] = param_name;
          is_variadic = TRUE;
          cur += 3;
          while (*cur == ' ' || *cur == '\t')
            cur++;
          if (*cur != ')')
            error_at(new_location(cur), "expected ')' after '...' in macro parameter list");
          cur++;
          break;
        }

        if (!is_ident_start_char(*cur)) {
          error_at(new_location(cur), "expected an identifier in macro parameter list");
        }
        char *param_start = cur;
        cur++;
        while (is_ident_char(*cur))
          cur++;
        int param_len = (int)(cur - param_start);
        char *param_name = malloc(param_len + 1);
        if (!param_name)
          error("memory allocation failed");
        memcpy(param_name, param_start, param_len);
        param_name[param_len] = '\0';

        // Support named variadic parameter: NAME...
        if (*cur == '.' && cur[1] == '.' && cur[2] == '.') {
          if (param_count >= param_cap) {
            param_cap = param_cap ? param_cap * 2 : 4;
            params = realloc(params, sizeof(char *) * param_cap);
            if (!params)
              error("memory allocation failed");
          }
          params[param_count++] = param_name;
          is_variadic = TRUE;
          cur += 3;
          while (*cur == ' ' || *cur == '\t')
            cur++;
          if (*cur != ')')
            error_at(new_location(cur), "expected ')' after variadic parameter");
          cur++;
          break;
        }

        if (param_count >= param_cap) {
          param_cap = param_cap ? param_cap * 2 : 4;
          params = realloc(params, sizeof(char *) * param_cap);
          if (!params)
            error("memory allocation failed");
        }
        params[param_count++] = param_name;

        while (*cur == ' ' || *cur == '\t')
          cur++;
        if (*cur == ',') {
          cur++;
          continue;
        }
        if (*cur == ')') {
          cur++;
          break;
        }
        error_at(new_location(cur), "expected ',' or ')' in macro parameter list");
      }
    }
  } else {
    while (*cur == ' ' || *cur == '\t')
      cur++;
  }

  int value_cap = 64;
  int value_len = 0;
  char *value = malloc(value_cap);
  if (!value)
    error("memory allocation failed");

  int in_string = FALSE;
  int in_char = FALSE;
  while (*cur) {
    // Handle line splicing first (backslash-newline)
    if (*cur == '\\') {
      if (cur[1] == '\n') {
        cur += 2;
        continue;
      }
      if (cur[1] == '\r' && cur[2] == '\n') {
        cur += 3;
        continue;
      }
    }
    if (!in_string && !in_char && *cur == '\n')
      break;
    if (!in_string && !in_char && startswith(cur, "//")) {
      while (*cur && *cur != '\n')
        cur++;
      break;
    }
    if (!in_string && !in_char && startswith(cur, "/*")) {
      char *end = strstr(cur + 2, "*/");
      if (!end)
        error_at(new_location(cur), "unclosed block comment in #define");
      if (value_len + 1 >= value_cap) {
        value_cap *= 2;
        value = realloc(value, value_cap);
        if (!value)
          error("memory allocation failed");
      }
      value[value_len++] = ' ';
      cur = end + 2;
      continue;
    }
    if (!in_char && *cur == '"') {
      in_string = !in_string;
      if (value_len + 1 >= value_cap) {
        value_cap *= 2;
        value = realloc(value, value_cap);
        if (!value)
          error("memory allocation failed");
      }
      value[value_len++] = *cur++;
      continue;
    }
    if (!in_string && *cur == '\'') {
      in_char = !in_char;
      if (value_len + 1 >= value_cap) {
        value_cap *= 2;
        value = realloc(value, value_cap);
        if (!value)
          error("memory allocation failed");
      }
      value[value_len++] = *cur++;
      continue;
    }
    if (in_string || in_char) {
      if (*cur == '\\' && *(cur + 1)) {
        if (value_len + 2 >= value_cap) {
          value_cap *= 2;
          value = realloc(value, value_cap);
          if (!value)
            error("memory allocation failed");
        }
        value[value_len++] = *cur++;
        value[value_len++] = *cur++;
        continue;
      }
    }
    if (value_len + 1 >= value_cap) {
      value_cap *= 2;
      value = realloc(value, value_cap);
      if (!value)
        error("memory allocation failed");
    }
    value[value_len++] = *cur++;
  }
  while (value_len > 0 && isspace((unsigned char)value[value_len - 1]))
    value_len--;
  if (value_len + 2 >= value_cap) {
    value_cap = value_len + 2;
    value = realloc(value, value_cap);
    if (!value)
      error("memory allocation failed");
  }
  value[value_len++] = '\n';
  value[value_len] = '\0';

  define_macro(name, value, params, param_count, is_function, is_variadic);

  if (*cur == '\n') {
    cur++;
  }
  *p = cur;
  return 1;
}

int parse_undef_directive(char **p) {
  char *cur = *p;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "undef") && !is_alnum(cur[5]))) {
    return 0;
  }

  cur += 5;
  while (*cur == ' ' || *cur == '\t')
    cur++;

  if (!is_ident_start_char(*cur)) {
    error_at(new_location(cur), "expected identifier after #undef");
  }

  char *name_start = cur;
  while (is_ident_char(*cur))
    cur++;
  int name_len = (int)(cur - name_start);

  char *rest = skip_trailing_spaces_and_comments(cur);
  if (*rest && *rest != '\n') {
    error_at(new_location(rest), "unexpected tokens after identifier in #undef");
  }

  undefine_macro(name_start, name_len);

  if (*rest == '\n')
    rest++;
  *p = rest;
  return 1;
}
