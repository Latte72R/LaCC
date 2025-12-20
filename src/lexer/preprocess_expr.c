#include "diagnostics.h"
#include "lexer.h"
#include "runtime.h"
#include "source.h"

#include "lexer_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void append_text(char **buf, int *len, int *cap, const char *text, int text_len) {
  if (*len + text_len >= *cap) {
    while (*len + text_len >= *cap)
      *cap *= 2;
    *buf = realloc(*buf, *cap);
    if (!*buf)
      error("memory allocation failed");
  }
  memcpy(*buf + *len, text, text_len);
  *len += text_len;
}

static void append_char(char **buf, int *len, int *cap, char c) { append_text(buf, len, cap, &c, 1); }

static char *copy_macro_body_text(Macro *macro) {
  char *body = macro->body;
  int len = (int)strlen(body);
  if (len > 0 && body[len - 1] == '\n')
    len--;
  char *result = malloc(len + 1);
  if (!result)
    error("memory allocation failed");
  if (len > 0)
    memcpy(result, body, len);
  result[len] = '\0';
  return result;
}

static int expr_expansion_depth = 0;

static int parse_header_name(const char *arg, int *is_system, char **out_name) {
  const char *p = skip_spaces(arg);
  if (*p == '<') {
    *is_system = 1;
    p++;
    const char *start = p;
    while (*p && *p != '>') {
      p++;
    }
    if (*p != '>')
      return 0;
    size_t len = p - start;
    char *name = malloc(len + 1);
    if (!name)
      error("memory allocation failed");
    if (len > 0)
      memcpy(name, start, len);
    name[len] = '\0';
    p++;
    p = skip_spaces(p);
    if (*p != '\0') {
      free(name);
      return 0;
    }
    *out_name = name;
    return 1;
  }
  if (*p == '"') {
    *is_system = 0;
    p++;
    const char *start = p;
    while (*p && *p != '"') {
      if (*p == '\\' && *(p + 1)) {
        p += 2;
        continue;
      }
      p++;
    }
    if (*p != '"')
      return 0;
    size_t len = p - start;
    char *name = malloc(len + 1);
    if (!name)
      error("memory allocation failed");
    if (len > 0)
      memcpy(name, start, len);
    name[len] = '\0';
    p++;
    p = skip_spaces(p);
    if (*p != '\0') {
      free(name);
      return 0;
    }
    *out_name = name;
    return 1;
  }
  return 0;
}

static int eval_has_include(const char *arg, int is_next) {
  char *expanded = expand_expression_internal(arg);
  int is_system = 0;
  char *name = NULL;
  int ok = parse_header_name(expanded, &is_system, &name);
  free(expanded);
  if (!ok || !name)
    return 0;
  char *resolved = NULL;
  char *src = NULL;
  if (is_next) {
    src = read_include_next_file(name, input_file, &resolved);
  } else {
    src = read_include_file(name, input_file, is_system, &resolved);
  }
  free(name);
  if (resolved)
    free(resolved);
  if (src) {
    free(src);
    return 1;
  }
  return 0;
}

char *expand_expression_internal(const char *expr) {
  expr_expansion_depth++;
  if (expr_expansion_depth > 256)
    error("macro expansion too deep in #if expression");
  int cap = (int)strlen(expr) + 32;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");
  int len = 0;
  const char *p = expr;

  while (*p) {
    if (isspace(*p)) {
      append_char(&buf, &len, &cap, *p);
      p++;
      continue;
    }

    if (is_ident_start_char(*p)) {
      const char *start = p;
      p++;
      while (is_ident_char(*p))
        p++;
      int ident_len = (int)(p - start);

      if (ident_len == 7 && !strncmp(start, "defined", 7) && !is_ident_char(*p)) {
        append_text(&buf, &len, &cap, start, ident_len);
        const char *q = p;
        while (isspace(*q)) {
          append_char(&buf, &len, &cap, *q);
          q++;
        }
        if (*q == '(') {
          append_char(&buf, &len, &cap, *q);
          q++;
          while (isspace(*q)) {
            append_char(&buf, &len, &cap, *q);
            q++;
          }
          const char *ident_start = q;
          if (!is_ident_start_char(*ident_start))
            error("expected identifier after defined(");
          while (is_ident_char(*q))
            q++;
          append_text(&buf, &len, &cap, ident_start, (int)(q - ident_start));
          while (isspace(*q)) {
            append_char(&buf, &len, &cap, *q);
            q++;
          }
          if (*q != ')')
            error("expected ')' after defined");
          append_char(&buf, &len, &cap, *q);
          q++;
          p = q;
          continue;
        } else {
          p = q;
          while (isspace(*p)) {
            append_char(&buf, &len, &cap, *p);
            p++;
          }
          if (!is_ident_start_char(*p))
            error("expected identifier after defined");
          const char *ident_start = p;
          while (is_ident_char(*p))
            p++;
          append_text(&buf, &len, &cap, ident_start, (int)(p - ident_start));
          continue;
        }
      }

      int is_has_include = (ident_len == 13 && !strncmp(start, "__has_include", 13));
      int is_has_include_next = (ident_len == 18 && !strncmp(start, "__has_include_next", 18));
      if (is_has_include || is_has_include_next) {
        const char *after_name = p;
        const char *ws = skip_spaces(after_name);
        if (*ws == '(') {
          Macro mm = {0};
          mm.name = (char *)(is_has_include ? "__has_include" : "__has_include_next");
          mm.param_count = 1;
          mm.is_function = 1;
          mm.is_variadic = 0;
          const char *arg_ptr = ws;
          int arg_cnt = 0;
          char **args = parse_macro_arguments(&arg_ptr, &mm, &arg_cnt);
          int has = (arg_cnt == 1) ? eval_has_include(args[0], is_has_include_next) : 0;
          const char *res = has ? "1" : "0";
          append_text(&buf, &len, &cap, res, 1);
          if (args) {
            for (int i = 0; i < arg_cnt; i++)
              free(args[i]);
            free(args);
          }
          p = arg_ptr;
          continue;
        }
      }

      Macro *macro = find_macro(start, ident_len);
      if (!macro || macro->is_expanding) {
        append_text(&buf, &len, &cap, start, ident_len);
        continue;
      }

      if (macro->is_function) {
        const char *after_name = p;
        const char *ws = skip_spaces(after_name);
        if (*ws != '(') {
          append_text(&buf, &len, &cap, start, ident_len);
          continue;
        }
        const char *arg_ptr = ws;
        int arg_cnt = 0;
        char **args = parse_macro_arguments(&arg_ptr, macro, &arg_cnt);
        if (macro->param_count == 1 && (ident_len == 13 && !strncmp(start, "__has_include", 13))) {
          int has = (arg_cnt == 1) ? eval_has_include(args[0], 0) : 0;
          const char *res = has ? "1" : "0";
          append_text(&buf, &len, &cap, res, 1);
          if (args) {
            for (int i = 0; i < arg_cnt; i++)
              free(args[i]);
            free(args);
          }
          p = arg_ptr;
          continue;
        }
        if (macro->param_count == 1 && (ident_len == 18 && !strncmp(start, "__has_include_next", 18))) {
          int has = (arg_cnt == 1) ? eval_has_include(args[0], 1) : 0;
          const char *res = has ? "1" : "0";
          append_text(&buf, &len, &cap, res, 1);
          if (args) {
            for (int i = 0; i < arg_cnt; i++)
              free(args[i]);
            free(args);
          }
          p = arg_ptr;
          continue;
        }
        macro->is_expanding = true;
        char *expanded_body = substitute_macro_body(macro, args, arg_cnt);
        macro->is_expanding = false;
        if (args) {
          for (int i = 0; i < arg_cnt; i++)
            free(args[i]);
          free(args);
        }
        char *expanded = expand_expression_internal(expanded_body);
        append_text(&buf, &len, &cap, expanded, (int)strlen(expanded));
        free(expanded_body);
        free(expanded);
        p = arg_ptr;
        continue;
      } else {
        macro->is_expanding = true;
        char *body_copy = copy_macro_body_text(macro);
        char *expanded = expand_expression_internal(body_copy);
        macro->is_expanding = false;
        append_text(&buf, &len, &cap, expanded, (int)strlen(expanded));
        free(body_copy);
        free(expanded);
        continue;
      }
    }

    if (*p == '"' || *p == '\'') {
      char quote = *p;
      append_char(&buf, &len, &cap, *p);
      p++;
      while (*p && *p != quote) {
        if (*p == '\\' && *(p + 1)) {
          append_text(&buf, &len, &cap, p, 2);
          p += 2;
          continue;
        }
        append_char(&buf, &len, &cap, *p);
        p++;
      }
      if (*p == quote) {
        append_char(&buf, &len, &cap, *p);
        p++;
      }
      continue;
    }

    append_char(&buf, &len, &cap, *p);
    p++;
  }

  if (len + 1 >= cap) {
    cap++;
    buf = realloc(buf, cap);
    if (!buf)
      error("memory allocation failed");
  }
  buf[len] = '\0';
  expr_expansion_depth--;
  return buf;
}
