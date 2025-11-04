#include "lacc.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Common externs used across helpers/table/builtins
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
  for (const char *p = start; p < end;) {
    // Handle line splices: backslash followed by newline (or CRLF)
    if (*p == '\\' && p + 1 < end) {
      if (p[1] == '\n') {
        p += 2;
        continue;
      }
      if (p[1] == '\r' && p + 2 < end && p[2] == '\n') {
        p += 3;
        continue;
      }
    }
    // Strip C and C++ style comments inside directive expressions
    if (p + 1 < end && p[0] == '/' && p[1] == '*') {
      p += 2;
      const char *q = p;
      while (q + 1 < end && !(q[0] == '*' && q[1] == '/'))
        q++;
      if (q + 1 >= end)
        error("unclosed block comment in directive expression");
      p = q + 2; // skip closing */
      // Insert a single space to avoid token pasting across removed comment
      buf[len++] = ' ';
      continue;
    }
    if (p + 1 < end && p[0] == '/' && p[1] == '/') {
      // Skip until end (the directive scanner already bounded by end-of-directive)
      // but in case of stray // within multi-line (after splicing), skip to end
      while (p < end && *p != '\n')
        p++;
      // Do not copy the newline; the outer scanner handles it. Insert space.
      if (p < end && *p == '\n')
        p++;
      buf[len++] = ' ';
      continue;
    }
    // Regular character
    buf[len++] = *p++;
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
  macro->is_function = false;
}

// internal, but used by sibling modules via extern
void define_macro(char *name, char *body, char **params, int param_count, int is_function, int is_variadic) {
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
  macro->is_variadic = is_variadic;
  macro->file = input_file;
  macro->is_expanding = false;
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
  define_macro(name_copy, body, NULL, 0, false, false);
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
  define_macro(name_copy, body, params, param_count, true, false);
}

void preprocess_initialize_builtins(void) {
  // コンパイラ識別マクロ
  define_builtin_object_macro("__LACC__", "1");

  // 標準規格識別マクロ
  define_builtin_object_macro("__STDC__", "1");
  define_builtin_object_macro("__STDC_VERSION__", "199901L");
  define_builtin_object_macro("__STDC_HOSTED__", "1");

  // ターゲット識別マクロ（OS/ABI）
#if LACC_PLATFORM_APPLE
  // macOS / Darwin (Mach-O)
  define_builtin_object_macro("__APPLE__", "1");
  define_builtin_object_macro("__MACH__", "1");
#else
  // Linux (ELF)
  define_builtin_object_macro("__linux__", "1");
  define_builtin_object_macro("__gnu_linux__", "1");
  define_builtin_object_macro("__ELF__", "1");
#endif
  define_builtin_object_macro("__x86_64__", "1");
  define_builtin_object_macro("__unix__", "1");
  define_builtin_object_macro("__GNUC__", "4");

  // 型同定マクロ（LP64）
  define_builtin_object_macro("__LP64__", "1");
  define_builtin_object_macro("__SIZE_TYPE__", "unsigned long");
  define_builtin_object_macro("__PTRDIFF_TYPE__", "long int");
  define_builtin_object_macro("__WCHAR_TYPE__", "int");
  define_builtin_object_macro("__WINT_TYPE__", "unsigned int");
  define_builtin_object_macro("__INTMAX_TYPE__", "long int");
  define_builtin_object_macro("__UINTMAX_TYPE__", "long unsigned int");
  define_builtin_object_macro("__INTPTR_TYPE__", "long int");
  define_builtin_object_macro("__UINTPTR_TYPE__", "long unsigned int");

  // ビット幅・エンディアン
  define_builtin_object_macro("__CHAR_BIT__", "8");
  define_builtin_object_macro("__ORDER_LITTLE_ENDIAN__", "1234");
  define_builtin_object_macro("__ORDER_BIG_ENDIAN__", "4321");
  define_builtin_object_macro("__ORDER_PDP_ENDIAN__", "3412");
  define_builtin_object_macro("__BYTE_ORDER__", "__ORDER_LITTLE_ENDIAN__");
  define_builtin_object_macro("__FLOAT_WORD_ORDER__", "__ORDER_LITTLE_ENDIAN__");

  // 整数境界 (LP64 想定)
  define_builtin_object_macro("__SCHAR_MAX__", "127");
  define_builtin_object_macro("__UCHAR_MAX__", "255");
  define_builtin_object_macro("__SHRT_MAX__", "32767");
  define_builtin_object_macro("__USHRT_MAX__", "65535");
  define_builtin_object_macro("__INT_MAX__", "2147483647");
  define_builtin_object_macro("__UINT_MAX__", "4294967295U");
  define_builtin_object_macro("__LONG_MAX__", "9223372036854775807L");
  define_builtin_object_macro("__ULONG_MAX__", "18446744073709551615UL");
  define_builtin_object_macro("__LONG_LONG_MAX__", "9223372036854775807LL");
  define_builtin_object_macro("__ULLONG_MAX__", "18446744073709551615ULL");
  define_builtin_object_macro("__SIZE_MAX__", "18446744073709551615UL");
  define_builtin_object_macro("__PTRDIFF_MAX__", "9223372036854775807L");
  define_builtin_object_macro("__INTPTR_MAX__", "9223372036854775807L");
  define_builtin_object_macro("__UINTPTR_MAX__", "18446744073709551615UL");
  define_builtin_object_macro("__WCHAR_MAX__", "2147483647");

  /* 機能検出は未対応なら 0 に落とす */
  define_builtin_function_macro("__has_attribute", 1, "0");
  define_builtin_function_macro("__has_builtin", 1, "0");

  /* 属性は関数形式で空展開 */
  define_builtin_function_macro("__attribute__", 1, "");
  define_builtin_function_macro("__nonnull", 1, "");
  define_builtin_object_macro("__wur", "");

  /* GNU 同義語は本体キーワードに写す */
  define_builtin_object_macro("__restrict__", "restrict");
  define_builtin_object_macro("__restrict", "restrict");
  define_builtin_object_macro("__volatile__", "volatile");
  define_builtin_object_macro("__inline__", "inline");
  define_builtin_object_macro("__inline", "inline");
  define_builtin_object_macro("__header_inline", "inline");
  define_builtin_object_macro("__signed__", "signed");
  define_builtin_object_macro("__const", "const");

  /* GCC の “構文緩和” */
  define_builtin_object_macro("__extension__", "");

  /* Clang系の機能プローブを未対応なら 0 に落とす */
  define_builtin_function_macro("__has_include", 1, "0");
  define_builtin_function_macro("__has_include_next", 1, "0");
  define_builtin_function_macro("__has_feature", 1, "0");
  define_builtin_function_macro("__has_extension", 1, "0");
}
