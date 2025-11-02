#include "lacc.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Needed from other preprocess module
extern char **parse_macro_arguments(const char **pp, Macro *macro, int *out_arg_count);
extern char *substitute_macro_body(Macro *macro, char **args, int arg_count);

// Local helpers duplicated from original implementation
static int is_ident_start_char(char c) { return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_'); }
static int is_ident_char(char c) { return is_ident_start_char(c) || ('0' <= c && c <= '9'); }

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

// Forward: macro lookup
extern Macro *find_macro(char *name, int len);

static int expr_expansion_depth = 0;

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
    if (isspace((unsigned char)*p)) {
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
        while (isspace((unsigned char)*q)) {
          append_char(&buf, &len, &cap, *q);
          q++;
        }
        if (*q == '(') {
          append_char(&buf, &len, &cap, *q);
          q++;
          while (isspace((unsigned char)*q)) {
            append_char(&buf, &len, &cap, *q);
            q++;
          }
          const char *ident_start = q;
          if (!is_ident_start_char(*ident_start))
            error("expected identifier after defined(");
          while (is_ident_char(*q))
            q++;
          append_text(&buf, &len, &cap, ident_start, (int)(q - ident_start));
          while (isspace((unsigned char)*q)) {
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
          while (isspace((unsigned char)*p)) {
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

      Macro *macro = find_macro((char *)start, ident_len);
      if (!macro || macro->is_expanding) {
        append_text(&buf, &len, &cap, start, ident_len);
        continue;
      }

      if (macro->is_function) {
        const char *after_name = p;
        const char *ws = after_name;
        while (isspace((unsigned char)*ws))
          ws++;
        if (*ws != '(') {
          append_text(&buf, &len, &cap, start, ident_len);
          continue;
        }
        const char *arg_ptr = ws;
        int arg_cnt = 0;
        char **args = parse_macro_arguments(&arg_ptr, macro, &arg_cnt);
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
