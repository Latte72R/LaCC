#include "lacc.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern char *input_file;
extern char *expand_expression_internal(const char *expr);

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

static void trim_trailing_whitespace(char *buf, int *len) {
  while (*len > 0 && isspace((unsigned char)buf[*len - 1]))
    (*len)--;
}

static void append_with_concat(char **buf, int *len, int *cap, const char *text, int text_len, int *pending_concat) {
  if (*pending_concat) {
    trim_trailing_whitespace(*buf, len);
    while (text_len > 0 && isspace((unsigned char)*text)) {
      text++;
      text_len--;
    }
    *pending_concat = false;
  }
  append_text(buf, len, cap, text, text_len);
}

static char *stringize_argument(char *arg) {
  int cap = (int)strlen(arg) * 4 + 3;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");
  int len = 0;
  buf[len++] = '"';
  int in_space = 1;
  for (char *p = arg; *p; p++) {
    char c = *p;
    if (c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v')
      c = ' ';
    if (isspace((unsigned char)c)) {
      if (in_space)
        continue;
      in_space = 1;
      c = ' ';
    } else {
      in_space = 0;
    }
    if (len + 2 >= cap) {
      cap *= 2;
      buf = realloc(buf, cap);
      if (!buf)
        error("memory allocation failed");
    }
    if (c == '\\' || c == '"')
      buf[len++] = '\\';
    buf[len++] = c;
  }
  buf[len++] = '"';
  buf[len] = '\0';
  return buf;
}

// Make this non-static to be used from preprocess expression evaluator
char *substitute_macro_body(Macro *macro, char **args, int arg_count) {
  char *body = macro->body;
  int cap = (int)strlen(body) + 32;
  int len = 0;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");

  int in_string = false;
  int in_char = false;
  int pending_concat = false;
  char **expanded_args = NULL;
  if (arg_count > 0) {
    expanded_args = malloc(sizeof(char *) * arg_count);
    if (!expanded_args)
      error("memory allocation failed");
    for (int i = 0; i < arg_count; i++) {
      if (!args || !args[i]) {
        expanded_args[i] = NULL;
      } else {
        expanded_args[i] = expand_expression_internal(args[i]);
      }
    }
  }

  for (char *p = body; *p;) {
    char ch = *p;

    if (in_string) {
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      if (ch == '\\' && *(p + 1)) {
        p++;
        ch = *p;
        append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      } else if (ch == '"') {
        in_string = false;
      }
      p++;
      continue;
    }

    if (in_char) {
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      if (ch == '\\' && *(p + 1)) {
        p++;
        ch = *p;
        append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      } else if (ch == '\'') {
        in_char = false;
      }
      p++;
      continue;
    }

    if (ch == '"') {
      in_string = true;
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      p++;
      continue;
    }

    if (ch == '\'') {
      in_char = true;
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      p++;
      continue;
    }

    if (ch == '#' && *(p + 1) == '#') {
      pending_concat = true;
      p += 2;
      while (isspace((unsigned char)*p))
        p++;
      continue;
    }

    if (ch == '#') {
      p++;
      while (isspace((unsigned char)*p))
        p++;
      if (!is_ident_start_char(*p))
        error("expected macro parameter after '#'");
      char *start = p;
      while (is_ident_char(*p))
        p++;
      int ident_len = (int)(p - start);
      int arg_index = -1;
      for (int i = 0; i < macro->param_count; i++) {
        if ((int)strlen(macro->params[i]) == ident_len && !strncmp(macro->params[i], start, ident_len)) {
          arg_index = i;
          break;
        }
      }
      if (arg_index < 0)
        error("expected macro parameter after '#'");
      char *arg = args[arg_index] ? args[arg_index] : "";
      char *stringized = stringize_argument(arg);
      append_with_concat(&buf, &len, &cap, stringized, (int)strlen(stringized), &pending_concat);
      free(stringized);
      continue;
    }

    if (is_ident_start_char(ch)) {
      char *start = p;
      p++;
      while (is_ident_char(*p))
        p++;
      int ident_len = (int)(p - start);
      int replaced = false;
      for (int i = 0; i < macro->param_count; i++) {
        if ((int)strlen(macro->params[i]) == ident_len && !strncmp(macro->params[i], start, ident_len)) {
          const char *arg_text = NULL;
          if (pending_concat) {
            arg_text = (args && args[i]) ? args[i] : "";
          } else if (expanded_args && expanded_args[i]) {
            arg_text = expanded_args[i];
          } else {
            arg_text = (args && args[i]) ? args[i] : "";
          }
          append_with_concat(&buf, &len, &cap, arg_text, (int)strlen(arg_text), &pending_concat);
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        append_with_concat(&buf, &len, &cap, start, ident_len, &pending_concat);
      }
      continue;
    }

    append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
    p++;
  }

  if (len + 1 >= cap) {
    cap += 1;
    buf = realloc(buf, cap);
    if (!buf)
      error("memory allocation failed");
  }
  buf[len] = '\0';

  if (expanded_args) {
    for (int i = 0; i < arg_count; i++) {
      if (expanded_args[i])
        free(expanded_args[i]);
    }
    free(expanded_args);
  }

  return buf;
}

char **parse_macro_arguments(const char **pp, Macro *macro, int *out_arg_count) {
  const char *p = *pp;
  if (*p != '(')
    error("expected '(' after macro name");
  p++;
  int depth = 1;
  int in_string = false;
  int in_char = false;
  const char *arg_start = p;
  int arg_cap = macro->param_count > 0 ? macro->param_count : 1;
  char **args = NULL;
  int arg_cnt = 0;

  if (macro->param_count > 0) {
    args = malloc(sizeof(char *) * arg_cap);
    if (!args)
      error("memory allocation failed");
  }

  int fixed_params = macro->is_variadic ? (macro->param_count - 1) : macro->param_count;
  int vararg_mode = false;

  while (*p) {
    char c = *p;
    if (in_string) {
      if (c == '\\' && *(p + 1)) {
        p += 2;
        continue;
      }
      if (c == '"')
        in_string = false;
      p++;
      continue;
    }
    if (in_char) {
      if (c == '\\' && *(p + 1)) {
        p += 2;
        continue;
      }
      if (c == '\'')
        in_char = false;
      p++;
      continue;
    }
    if (c == '"') {
      in_string = true;
      p++;
      continue;
    }
    if (c == '\'') {
      in_char = true;
      p++;
      continue;
    }
    if (c == '(') {
      depth++;
      p++;
      continue;
    }
    if (c == ')') {
      depth--;
      if (depth == 0) {
        if (macro->param_count > 0) {
          int len = (int)(p - arg_start);
          char *arg = malloc(len + 1);
          if (!arg)
            error("memory allocation failed");
          memcpy(arg, arg_start, len);
          arg[len] = '\0';
          if (arg_cnt >= arg_cap) {
            arg_cap *= 2;
            args = realloc(args, sizeof(char *) * arg_cap);
            if (!args)
              error("memory allocation failed");
          }
          args[arg_cnt++] = arg;
          // If variadic and no varargs provided, ensure we still have the correct number of args
          if (macro->is_variadic && arg_cnt < macro->param_count) {
            // Append empty vararg
            char *empty = malloc(1);
            if (!empty)
              error("memory allocation failed");
            empty[0] = '\0';
            if (arg_cnt >= arg_cap) {
              arg_cap *= 2;
              args = realloc(args, sizeof(char *) * arg_cap);
              if (!args)
                error("memory allocation failed");
            }
            args[arg_cnt++] = empty;
          }
        } else {
          for (const char *q = arg_start; q < p; q++) {
            if (!isspace((unsigned char)*q))
              error("macro %s takes no arguments", macro->name);
          }
        }
        p++;
        break;
      }
      p++;
      continue;
    }
    if (c == ',' && depth == 1) {
      if (macro->param_count == 0)
        error("macro %s takes no arguments", macro->name);
      if (!macro->is_variadic) {
        int len = (int)(p - arg_start);
        char *arg = malloc(len + 1);
        if (!arg)
          error("memory allocation failed");
        memcpy(arg, arg_start, len);
        arg[len] = '\0';
        if (arg_cnt >= arg_cap) {
          arg_cap *= 2;
          args = realloc(args, sizeof(char *) * arg_cap);
          if (!args)
            error("memory allocation failed");
        }
        args[arg_cnt++] = arg;
        p++;
        arg_start = p;
        continue;
      } else {
        if (!vararg_mode && arg_cnt < fixed_params - 1) {
          // Still collecting fixed params (not including the last fixed)
          int len = (int)(p - arg_start);
          char *arg = malloc(len + 1);
          if (!arg)
            error("memory allocation failed");
          memcpy(arg, arg_start, len);
          arg[len] = '\0';
          if (arg_cnt >= arg_cap) {
            arg_cap *= 2;
            args = realloc(args, sizeof(char *) * arg_cap);
            if (!args)
              error("memory allocation failed");
          }
          args[arg_cnt++] = arg;
          p++;
          arg_start = p;
          continue;
        } else if (!vararg_mode && arg_cnt == fixed_params - 1) {
          // This comma ends the last fixed param, next is vararg
          int len = (int)(p - arg_start);
          char *arg = malloc(len + 1);
          if (!arg)
            error("memory allocation failed");
          memcpy(arg, arg_start, len);
          arg[len] = '\0';
          if (arg_cnt >= arg_cap) {
            arg_cap *= 2;
            args = realloc(args, sizeof(char *) * arg_cap);
            if (!args)
              error("memory allocation failed");
          }
          args[arg_cnt++] = arg;
          p++;
          arg_start = p;
          vararg_mode = true;
          continue;
        } else {
          // Already in vararg: do not split on commas
          p++;
          continue;
        }
      }
    }
    p++;
  }

  if (depth != 0)
    error("unterminated macro argument list");

  *pp = p;
  *out_arg_count = arg_cnt;
  return args;
}

void expand_macro(Macro *macro, char **args, int arg_count) {
  if (!macro) {
    return;
  }

  if (macro->is_expanding) {
    // Prevent infinite recursion per C macro re-expansion rules: skip if currently expanding
    return;
  }

  char *saved_input = user_input;
  char *saved_file = input_file;

  macro->is_expanding = true;
  char *expanded_input = macro->body;

  if (macro->is_function) {
    if (macro->param_count != arg_count) {
      error("macro %s expects %d arguments but %d given", macro->name, macro->param_count, arg_count);
    }
    expanded_input = substitute_macro_body(macro, args, arg_count);
    CharPtrList *node = malloc(sizeof(CharPtrList));
    if (!node)
      error("memory allocation failed");
    node->str = expanded_input;
    node->next = user_input_list;
    user_input_list = node;
  }

  user_input = expanded_input;
  tokenize();
  macro->is_expanding = false;

  user_input = saved_input;
  input_file = saved_file;
}
