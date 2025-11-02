#include "lacc.h"

// Common externs used across helpers/table/builtins
extern const int TRUE;
extern const int FALSE;
extern void *NULL;
extern Macro *macros;
extern char *input_file;

// ============================
// Helpers (exported)
// ============================

int is_ident_start_char(char c) { return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_'); }

int is_ident_char(char c) { return is_ident_start_char(c) || ('0' <= c && c <= '9'); }

char *duplicate_cstring(const char *src) {
  int len = (int)strlen(src);
  char *copy = malloc(len + 1);
  if (!copy)
    error("memory allocation failed");
  memcpy(copy, src, len + 1);
  return copy;
}

char *copy_trim(const char *start, const char *end) {
  while (start < end && isspace((unsigned char)*start))
    start++;
  while (end > start && isspace((unsigned char)*(end - 1)))
    end--;
  int len = (int)(end - start);
  char *result = malloc(len + 1);
  if (!result)
    error("memory allocation failed");
  if (len > 0)
    memcpy(result, start, len);
  result[len] = '\0';
  return result;
}

char *copy_trim_directive_expr(const char *start, const char *end) {
  int cap = (int)(end - start) + 1;
  if (cap < 1)
    cap = 1;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");
  int len = 0;
  for (const char *p = start; p < end; p++) {
    if (*p == '\\' && p + 1 < end) {
      if (p[1] == '\n') {
        p++;
        continue;
      }
      if (p[1] == '\r' && p + 2 < end && p[2] == '\n') {
        p += 2;
        continue;
      }
    }
    buf[len++] = *p;
  }
  int front = 0;
  while (front < len && isspace((unsigned char)buf[front]))
    front++;
  int back = len;
  while (back > front && isspace((unsigned char)buf[back - 1]))
    back--;
  int trimmed_len = back - front;
  char *result = malloc(trimmed_len + 1);
  if (!result)
    error("memory allocation failed");
  if (trimmed_len > 0)
    memcpy(result, buf + front, trimmed_len);
  result[trimmed_len] = '\0';
  free(buf);
  return result;
}

char *skip_trailing_spaces_and_comments(char *cur) {
  while (*cur && *cur != '\n') {
    if (isspace((unsigned char)*cur)) {
      cur++;
      continue;
    }
    if (startswith(cur, "//")) {
      while (*cur && *cur != '\n')
        cur++;
      break;
    }
    if (startswith(cur, "/*")) {
      char *end = strstr(cur + 2, "*/");
      if (!end)
        error_at(new_location(cur), "unclosed block comment in directive");
      cur = end + 2;
      continue;
    }
    break;
  }
  return cur;
}

// ============================
// Macro table (find/define/undef)
// ============================

// public
Macro *find_macro(char *name, int len) {
  for (Macro *macro = macros; macro; macro = macro->next) {
    if ((int)strlen(macro->name) == len && !strncmp(macro->name, name, len)) {
      return macro;
    }
  }
  return NULL;
}

static void free_macro_contents(Macro *macro) {
  if (macro->name)
    free(macro->name);
  if (macro->body)
    free(macro->body);
  if (macro->params) {
    for (int i = 0; i < macro->param_count; i++)
      free(macro->params[i]);
    free(macro->params);
  }
  macro->params = NULL;
  macro->param_count = 0;
  macro->is_function = FALSE;
}

// internal, but used by sibling modules via extern
void define_macro(char *name, char *body, char **params, int param_count, int is_function) {
  Macro *macro = find_macro(name, (int)strlen(name));
  if (macro) {
    free_macro_contents(macro);
  } else {
    macro = malloc(sizeof(Macro));
    if (!macro)
      error("memory allocation failed");
    macro->next = macros;
    macros = macro;
  }
  macro->name = name;
  macro->body = body;
  macro->params = params;
  macro->param_count = param_count;
  macro->is_function = is_function;
  macro->file = input_file;
  macro->is_expanding = FALSE;
}

void undefine_macro(char *name, int len) {
  Macro **link = &macros;
  while (*link) {
    Macro *macro = *link;
    if ((int)strlen(macro->name) == len && !strncmp(macro->name, name, len)) {
      *link = macro->next;
      free_macro_contents(macro);
      free(macro);
      return;
    }
    link = &macro->next;
  }
}

// ============================
// Builtins (predefined macros)
// ============================

static void define_builtin_object_macro(const char *name, const char *value) {
  char *name_copy = duplicate_cstring(name);
  int len = (int)strlen(value);
  char *body = malloc(len + 2);
  if (!body)
    error("memory allocation failed");
  memcpy(body, value, len);
  body[len] = '\n';
  body[len + 1] = '\0';
  define_macro(name_copy, body, NULL, 0, FALSE);
}

static void define_builtin_function_macro(const char *name, int param_count, const char *value) {
  char *name_copy = duplicate_cstring(name);
  int vlen = (int)strlen(value);
  char *body = malloc(vlen + 2);
  if (!body)
    error("memory allocation failed");
  memcpy(body, value, vlen);
  body[vlen] = '\n';
  body[vlen + 1] = '\0';

  char **params = NULL;
  if (param_count > 0) {
    params = malloc(sizeof(char *) * param_count);
    if (!params)
      error("memory allocation failed");
    for (int i = 0; i < param_count; i++) {
      char buf[8];
      int n = snprintf(buf, sizeof(buf), "p%d", i);
      char *pname = malloc(n + 1);
      if (!pname)
        error("memory allocation failed");
      memcpy(pname, buf, n + 1);
      params[i] = pname;
    }
  }
  define_macro(name_copy, body, params, param_count, TRUE);
}

void preprocess_initialize_builtins(void) {
  define_builtin_object_macro("__x86_64__", "1");
  define_builtin_object_macro("__LP64__", "1");
  define_builtin_object_macro("__LACC__", "1");
  define_builtin_object_macro("__THROW", "");
  define_builtin_object_macro("__restrict", "");
  define_builtin_object_macro("__restrict__", "");
  define_builtin_object_macro("restrict", "");
  define_builtin_function_macro("__attribute__", 1, "");
  define_builtin_function_macro("__nonnull", 1, "");
}
