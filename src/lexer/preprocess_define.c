#include "diagnostics.h"
#include "lexer.h"

#include "lexer_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int parse_define_directive(char **p) {
  char *cur = *p;
  if (!consume_directive_keyword(&cur, "define"))
    return 0;

  if (!is_ident_start_char(*cur)) {
    error_at(new_location(cur), "expected an identifier after #define");
  }

  char *name_start = cur;
  while (is_ident_char(*cur))
    cur++;
  size_t name_len = cur - name_start;
  char *name = malloc(name_len + 1);
  if (!name)
    error("memory allocation failed");
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  int is_function = false;
  int is_variadic = false;
  char **params = NULL;
  int param_count = 0;
  int param_cap = 0;

  if (*cur == '(') {
    is_function = true;
    cur++;
    if (*cur == ')') {
      cur++;
    } else {
      while (true) {
        cur = skip_directive_spaces(cur);
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
          is_variadic = true;
          cur += 3;
          cur = skip_directive_spaces(cur);
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
        size_t param_len = cur - param_start;
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
          is_variadic = true;
          cur += 3;
          cur = skip_directive_spaces(cur);
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

        cur = skip_directive_spaces(cur);
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
    cur = skip_directive_spaces(cur);
  }

  int value_cap = 64;
  int value_len = 0;
  char *value = malloc(value_cap);
  if (!value)
    error("memory allocation failed");

  int in_string = false;
  int in_char = false;
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
  while (value_len > 0 && isspace(value[value_len - 1]))
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
  if (!consume_directive_keyword(&cur, "undef"))
    return 0;

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
