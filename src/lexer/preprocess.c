
#include "lacc.h"

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;
extern Macro *macros;

// マクロ名に一致する定義を検索する
Macro *find_macro(char *name, int len) {
  for (Macro *macro = macros; macro; macro = macro->next) {
    if ((int)strlen(macro->name) == len && !strncmp(macro->name, name, len)) {
      return macro;
    }
  }
  return NULL;
}

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
static int expr_expansion_depth = 0;

static char *skip_trailing_spaces_and_comments(char *cur);

// 既存マクロの内容（名前・本体・引数）を解放する
static void free_macro_contents(Macro *macro) {
  if (macro->name) {
    free(macro->name);
  }
  if (macro->body) {
    free(macro->body);
  }
  if (macro->params) {
    for (int i = 0; i < macro->param_count; i++) {
      free(macro->params[i]);
    }
    free(macro->params);
  }
  macro->params = NULL;
  macro->param_count = 0;
  macro->is_function = FALSE;
}

// マクロを新規作成または再定義する
static void define_macro(char *name, char *body, char **params, int param_count, int is_function) {
  Macro *macro = find_macro(name, strlen(name));
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

static char *duplicate_cstring(const char *src) {
  int len = strlen(src);
  char *copy = malloc(len + 1);
  if (!copy)
    error("memory allocation failed");
  memcpy(copy, src, len + 1);
  return copy;
}

static void define_builtin_object_macro(const char *name, const char *value) {
  char *name_copy = duplicate_cstring(name);
  int len = strlen(value);
  char *body = malloc(len + 2);
  if (!body)
    error("memory allocation failed");
  memcpy(body, value, len);
  body[len] = '\n';
  body[len + 1] = '\0';
  define_macro(name_copy, body, NULL, 0, FALSE);
}

void preprocess_initialize_builtins(void) {
  define_builtin_object_macro("__x86_64__", "1");
  define_builtin_object_macro("__LP64__", "1");
  define_builtin_object_macro("__LACC__", "1");
}

// マクロ定義を削除する
static void undefine_macro(char *name, int len) {
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

// 識別子の先頭として有効な文字か判定する
static int is_ident_start_char(char c) { return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_'); }

// 識別子の構成文字として有効か判定する
static int is_ident_char(char c) { return is_ident_start_char(c) || ('0' <= c && c <= '9'); }

// バッファに文字列を追記（必要なら再確保）する
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

// バッファ末尾の空白類を削除する
static void trim_trailing_whitespace(char *buf, int *len) {
  while (*len > 0 && isspace((unsigned char)buf[*len - 1]))
    (*len)--;
}

// '##' による連結を考慮してテキストを追記する
static void append_with_concat(char **buf, int *len, int *cap, const char *text, int text_len, int *pending_concat) {
  if (*pending_concat) {
    trim_trailing_whitespace(*buf, len);
    while (text_len > 0 && isspace((unsigned char)*text)) {
      text++;
      text_len--;
    }
    *pending_concat = FALSE;
  }
  append_text(buf, len, cap, text, text_len);
}

// マクロ引数を文字列化（# 演算子）する
static char *stringize_argument(char *arg) {
  int cap = strlen(arg) * 4 + 3;
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

// マクロ本体中のパラメータを引数で置換する
static char *substitute_macro_body(Macro *macro, char **args, int arg_count) {
  char *body = macro->body;
  int cap = strlen(body) + 32;
  int len = 0;
  char *buf = malloc(cap);
  if (!buf)
    error("memory allocation failed");

  int in_string = FALSE;
  int in_char = FALSE;
  int pending_concat = FALSE;

  for (char *p = body; *p;) {
    char ch = *p;

    if (in_string) {
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      if (ch == '\\' && *(p + 1)) {
        p++;
        ch = *p;
        append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      } else if (ch == '"') {
        in_string = FALSE;
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
        in_char = FALSE;
      }
      p++;
      continue;
    }

    if (ch == '"') {
      in_string = TRUE;
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      p++;
      continue;
    }

    if (ch == '\'') {
      in_char = TRUE;
      append_with_concat(&buf, &len, &cap, &ch, 1, &pending_concat);
      p++;
      continue;
    }

    if (ch == '#' && *(p + 1) == '#') {
      pending_concat = TRUE;
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
      int ident_len = p - start;
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
      append_with_concat(&buf, &len, &cap, stringized, strlen(stringized), &pending_concat);
      free(stringized);
      continue;
    }

    if (is_ident_start_char(ch)) {
      char *start = p;
      p++;
      while (is_ident_char(*p))
        p++;
      int ident_len = p - start;
      int replaced = FALSE;
      for (int i = 0; i < macro->param_count; i++) {
        if ((int)strlen(macro->params[i]) == ident_len && !strncmp(macro->params[i], start, ident_len)) {
          char *arg = args[i] ? args[i] : "";
          append_with_concat(&buf, &len, &cap, arg, strlen(arg), &pending_concat);
          replaced = TRUE;
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
  return buf;
}

// マクロを展開して入力としてトークナイズさせる
void expand_macro(Macro *macro, char **args, int arg_count) {
  if (!macro) {
    return;
  }

  if (macro->is_expanding) {
    error("recursive macro expansion: %s", macro->name);
  }

  char *saved_input = user_input;
  char *saved_file = input_file;

  macro->is_expanding = TRUE;
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
  if (macro->file) {
    input_file = macro->file;
  }
  tokenize();
  macro->is_expanding = FALSE;

  user_input = saved_input;
  input_file = saved_file;
}

// #include で指定されたファイルを読み込み、トークナイズする
void handle_include_directive(char *name, char *p, int is_system_header) {
  char *including_file = input_file;
  char *raw_name = name;
  char *resolved_path = NULL;
  char *new_input = read_include_file(raw_name, including_file, is_system_header, &resolved_path);
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open include file: %s", raw_name);
  }
  if (!resolved_path)
    resolved_path = duplicate_cstring(raw_name);
  free(raw_name);

  if (!resolved_path)
    resolved_path = duplicate_cstring("");

  for (FileName *s = filenames; s; s = s->next) {
    if (!strcmp(s->name, resolved_path)) {
      free(resolved_path);
      free(new_input);
      return;
    }
  }

  char *input_file_prev = input_file;
  input_file = resolved_path;
  FileName *filename = malloc(sizeof(FileName));
  filename->name = input_file;
  filename->next = filenames;
  filenames = filename;

  char *user_input_prev = user_input;
  CharPtrList *user_input_list_prev = user_input_list;
  user_input_list = malloc(sizeof(CharPtrList));
  if (!user_input_list)
    error("memory allocation failed");
  user_input_list->str = new_input;
  user_input_list->next = user_input_list_prev;

  user_input = new_input;
  tokenize();

  user_input = user_input_prev;
  input_file = input_file_prev;
}

// 文字列から #include 行を解析して処理する
int parse_include_directive(char **p) {
  char *cur = *p;
  if (*cur != '#') {
    return 0;
  }
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;

  if (!(startswith(cur, "include") && !is_alnum(cur[7]))) {
    return 0;
  }

  cur += 7;
  while (*cur == ' ' || *cur == '\t')
    cur++;

  char close;
  if (*cur == '"') {
    close = '"';
  } else if (*cur == '<') {
    close = '>';
  } else {
    error_at(new_location(cur), "expected '\"' or '<' after #include");
  }

  cur++;
  char *name_start = cur;
  while (*cur && *cur != close) {
    if (close == '"' && *cur == '\\') {
      cur++;
      if (!*cur)
        error_at(new_location(name_start), "unclosed string literal [in tokenize]");
      cur++;
      continue;
    }
    if (*cur == '\n')
      error_at(new_location(cur), "unexpected newline in #include directive");
    cur++;
  }

  if (*cur != close) {
    error_at(new_location(name_start - 1), "unterminated include filename");
  }

  int len = cur - name_start;
  char *name = malloc(len + 1);
  if (!name)
    error("memory allocation failed");
  if (len > 0)
    memcpy(name, name_start, len);
  name[len] = '\0';

  cur++; // skip closing delimiter
  char *rest = skip_trailing_spaces_and_comments(cur);
  if (*rest && *rest != '\n') {
    error_at(new_location(rest), "unexpected tokens after #include filename");
  }

  int is_system_header = (close == '>');
  handle_include_directive(name, name_start, is_system_header);

  if (*rest == '\n')
    rest++;
  *p = rest;
  return 1;
}

// 文字列から #define 行を解析し、マクロを登録する
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
  while (is_ident_char(*cur)) {
    cur++;
  }
  int name_len = cur - name_start;
  char *name = malloc(name_len + 1);
  if (!name)
    error("memory allocation failed");
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  int is_function = FALSE;
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
        if (!is_ident_start_char(*cur)) {
          error_at(new_location(cur), "expected an identifier in macro parameter list");
        }
        char *param_start = cur;
        cur++;
        while (is_ident_char(*cur))
          cur++;
        int param_len = cur - param_start;
        char *param_name = malloc(param_len + 1);
        if (!param_name)
          error("memory allocation failed");
        memcpy(param_name, param_start, param_len);
        param_name[param_len] = '\0';

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

    // End of directive line if not inside a block comment or string/char
    if (!in_string && !in_char && *cur == '\n')
      break;

    // Skip line comments (//...) to end of physical line
    if (!in_string && !in_char && startswith(cur, "//")) {
      while (*cur && *cur != '\n')
        cur++;
      break;
    }

    // Skip block comments and treat them as single space
    if (!in_string && !in_char && startswith(cur, "/*")) {
      char *end = strstr(cur + 2, "*/");
      if (!end)
        error_at(new_location(cur), "unclosed block comment in #define");
      // Append a single space to separate tokens if needed
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

    // Handle entering/exiting string literal
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

    // Inside string/char, handle escapes and copy verbatim
    if (in_string || in_char) {
      if (*cur == '\\' && *(cur + 1)) {
        // Copy escape sequence as-is
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
      if ((in_string && *cur == '"') || (in_char && *cur == '\'')) {
        // Toggle state will be handled above on next iteration; just fall through
      }
    }

    if (value_len + 1 >= value_cap) {
      value_cap *= 2;
      value = realloc(value, value_cap);
      if (!value)
        error("memory allocation failed");
    }
    value[value_len++] = *cur;
    cur++;
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

  define_macro(name, value, params, param_count, is_function);

  if (*cur == '\n') {
    cur++;
  }
  *p = cur;
  return 1;
}

// #ifdef 行を解析して条件付きコンパイルを処理する
int parse_ifdef_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "ifdef") && !is_alnum(cur[5]))) {
    return 0;
  }

  char *name_start = cur + 5;
  while (*name_start == ' ' || *name_start == '\t')
    name_start++;

  if (!is_ident_start_char(*name_start)) {
    error_at(new_location(name_start), "expected identifier after #ifdef");
  }

  char *name_end = name_start;
  while (is_ident_char(*name_end))
    name_end++;
  int name_len = name_end - name_start;

  char *rest = skip_trailing_spaces_and_comments(name_end);
  if (*rest && *rest != '\n') {
    error_at(new_location(rest), "unexpected tokens after identifier in #ifdef");
  }

  int ignoring = skip_depth > 0;
  int cond = FALSE;
  if (!ignoring) {
    cond = find_macro(name_start, name_len) != NULL;
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

// #ifndef 行を解析して条件付きコンパイルを処理する
int parse_ifndef_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "ifndef") && !is_alnum(cur[6]))) {
    return 0;
  }

  char *name_start = cur + 6;
  while (*name_start == ' ' || *name_start == '\t')
    name_start++;

  if (!is_ident_start_char(*name_start)) {
    error_at(new_location(name_start), "expected identifier after #ifndef");
  }

  char *name_end = name_start;
  while (is_ident_char(*name_end))
    name_end++;
  int name_len = name_end - name_start;

  char *rest = skip_trailing_spaces_and_comments(name_end);
  if (*rest && *rest != '\n') {
    error_at(new_location(rest), "unexpected tokens after identifier in #ifndef");
  }

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

// #undef 行を解析してマクロ定義を削除する
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
  int name_len = cur - name_start;

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

// 条件式の前後から空白を削除して複製する
static char *copy_trim(const char *start, const char *end) {
  while (start < end && isspace((unsigned char)*start))
    start++;
  while (end > start && isspace((unsigned char)*(end - 1)))
    end--;
  int len = end - start;
  char *result = malloc(len + 1);
  if (!result)
    error("memory allocation failed");
  if (len > 0)
    memcpy(result, start, len);
  result[len] = '\0';
  return result;
}

// ディレクティブの式からバックスラッシュ改行を除去しつつトリムする
static char *copy_trim_directive_expr(const char *start, const char *end) {
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

// ディレクティブ行末までの空白およびコメントを読み飛ばす
static char *skip_trailing_spaces_and_comments(char *cur) {
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

// 末尾改行を除いたマクロ本体を複製する
static char *copy_macro_body_text(Macro *macro) {
  char *body = macro->body;
  int len = strlen(body);
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

// 関数型マクロの引数文字列を分割して保持する
char **parse_macro_arguments(const char **pp, Macro *macro, int *out_arg_count) {
  const char *p = *pp;
  if (*p != '(')
    error("expected '(' after macro name");
  p++;
  int depth = 1;
  int in_string = FALSE;
  int in_char = FALSE;
  const char *arg_start = p;
  int arg_cap = macro->param_count > 0 ? macro->param_count : 1;
  char **args = NULL;
  int arg_cnt = 0;

  if (macro->param_count > 0) {
    args = malloc(sizeof(char *) * arg_cap);
    if (!args)
      error("memory allocation failed");
  }

  while (*p) {
    char c = *p;
    if (in_string) {
      if (c == '\\' && *(p + 1)) {
        p += 2;
        continue;
      }
      if (c == '"')
        in_string = FALSE;
      p++;
      continue;
    }
    if (in_char) {
      if (c == '\\' && *(p + 1)) {
        p += 2;
        continue;
      }
      if (c == '\'')
        in_char = FALSE;
      p++;
      continue;
    }
    if (c == '"') {
      in_string = TRUE;
      p++;
      continue;
    }
    if (c == '\'') {
      in_char = TRUE;
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
          char *arg = copy_trim(arg_start, p);
          if (arg_cnt >= arg_cap) {
            arg_cap *= 2;
            args = realloc(args, sizeof(char *) * arg_cap);
            if (!args)
              error("memory allocation failed");
          }
          args[arg_cnt++] = arg;
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
      char *arg = copy_trim(arg_start, p);
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
    }
    p++;
  }

  if (depth != 0)
    error("unterminated macro argument list");

  *pp = p;
  *out_arg_count = arg_cnt;
  return args;
}

static char *expand_expression_internal(const char *expr);

// 1 文字をバッファに追記する
static void append_char(char **buf, int *len, int *cap, char c) { append_text(buf, len, cap, &c, 1); }

// #if 条件式に含まれる識別子やマクロを再帰的に展開する
static char *expand_expression_internal(const char *expr) {
  expr_expansion_depth++;
  if (expr_expansion_depth > 256)
    error("macro expansion too deep in #if expression");
  int cap = strlen(expr) + 32;
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
      int ident_len = p - start;

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
          append_text(&buf, &len, &cap, ident_start, q - ident_start);
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
          append_text(&buf, &len, &cap, ident_start, p - ident_start);
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
        macro->is_expanding = TRUE;
        char *expanded_body = substitute_macro_body(macro, args, arg_cnt);
        macro->is_expanding = FALSE;
        if (args) {
          for (int i = 0; i < arg_cnt; i++)
            free(args[i]);
          free(args);
        }
        char *expanded = expand_expression_internal(expanded_body);
        append_text(&buf, &len, &cap, expanded, strlen(expanded));
        free(expanded_body);
        free(expanded);
        p = arg_ptr;
        continue;
      } else {
        macro->is_expanding = TRUE;
        char *body_copy = copy_macro_body_text(macro);
        char *expanded = expand_expression_internal(body_copy);
        macro->is_expanding = FALSE;
        append_text(&buf, &len, &cap, expanded, strlen(expanded));
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

typedef struct {
  const char *p;
} ExprParser;

// 式パーサで空白を読み飛ばす
static void skip_spaces_expr(ExprParser *ctx) {
  while (isspace((unsigned char)*ctx->p))
    ctx->p++;
}

static long long parse_conditional(ExprParser *ctx, int *ok);

// 前処理式の数値リテラルを読み取る（2 進数接頭辞対応）
static long long parse_number_literal_pp(ExprParser *ctx, int *ok) {
  const char *p = ctx->p;
  if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
    p += 2;
    if (*p != '0' && *p != '1') {
      *ok = FALSE;
      return 0;
    }
    long long val = 0;
    while (*p == '0' || *p == '1') {
      val = (val << 1) | (*p - '0');
      p++;
    }
    ctx->p = p;
    while (*ctx->p == 'u' || *ctx->p == 'U' || *ctx->p == 'l' || *ctx->p == 'L')
      ctx->p++;
    return val;
  }

  char *end;
  long val_long = strtol(p, &end, 0);
  if (end == p) {
    *ok = FALSE;
    return 0;
  }
  ctx->p = end;
  while (*ctx->p == 'u' || *ctx->p == 'U' || *ctx->p == 'l' || *ctx->p == 'L')
    ctx->p++;
  return (long long)val_long;
}

// 前処理式の文字リテラルを読み取る（単純なエスケープ対応）
static long long parse_char_literal_pp(ExprParser *ctx, int *ok) {
  const char *p = ctx->p;
  if (*p != '\'') {
    *ok = FALSE;
    return 0;
  }
  p++;
  if (*p == '\0') {
    *ok = FALSE;
    return 0;
  }
  long long val;
  if (*p == '\\') {
    p++;
    switch (*p) {
    case 'a':
      val = '\a';
      break;
    case 'b':
      val = '\b';
      break;
    case 'e':
      val = '\e';
      break;
    case 'f':
      val = '\f';
      break;
    case 'n':
      val = '\n';
      break;
    case 'r':
      val = '\r';
      break;
    case 't':
      val = '\t';
      break;
    case 'v':
      val = '\v';
      break;
    case '\\':
      val = '\\';
      break;
    case '\'':
      val = '\'';
      break;
    case '\"':
      val = '\"';
      break;
    case '0':
      val = '\0';
      break;
    default:
      *ok = FALSE;
      return 0;
    }
    p++;
  } else {
    val = (unsigned char)*p;
    p++;
  }
  if (*p != '\'') {
    *ok = FALSE;
    return 0;
  }
  p++;
  ctx->p = p;
  return val;
}

static long long parse_primary(ExprParser *ctx, int *ok);
static long long parse_unary(ExprParser *ctx, int *ok);
static long long parse_mul(ExprParser *ctx, int *ok);
static long long parse_add(ExprParser *ctx, int *ok);
static long long parse_shift(ExprParser *ctx, int *ok);
static long long parse_relational(ExprParser *ctx, int *ok);
static long long parse_equality(ExprParser *ctx, int *ok);
static long long parse_bitand(ExprParser *ctx, int *ok);
static long long parse_bitxor(ExprParser *ctx, int *ok);
static long long parse_bitor(ExprParser *ctx, int *ok);
static long long parse_logical_and(ExprParser *ctx, int *ok);
static long long parse_logical_or(ExprParser *ctx, int *ok);

// 前処理式の最小単位（defined/括弧/数値/文字/識別子）を解析する
static long long parse_primary(ExprParser *ctx, int *ok) {
  skip_spaces_expr(ctx);
  if (startswith((char *)ctx->p, "defined") && !is_ident_char(ctx->p[7])) {
    ctx->p += 7;
    skip_spaces_expr(ctx);
    int has_paren = FALSE;
    if (*ctx->p == '(') {
      has_paren = TRUE;
      ctx->p++;
      skip_spaces_expr(ctx);
    }
    if (!is_ident_start_char(*ctx->p)) {
      *ok = FALSE;
      return 0;
    }
    const char *ident_start = ctx->p;
    ctx->p++;
    while (is_ident_char(*ctx->p))
      ctx->p++;
    int ident_len = ctx->p - ident_start;
    if (has_paren) {
      skip_spaces_expr(ctx);
      if (*ctx->p != ')') {
        *ok = FALSE;
        return 0;
      }
      ctx->p++;
    }
    Macro *macro = find_macro((char *)ident_start, ident_len);
    return macro ? 1 : 0;
  }

  if (*ctx->p == '(') {
    ctx->p++;
    long long val = parse_conditional(ctx, ok);
    if (!*ok)
      return 0;
    skip_spaces_expr(ctx);
    if (*ctx->p != ')') {
      *ok = FALSE;
      return 0;
    }
    ctx->p++;
    return val;
  }

  if (*ctx->p == '\'') {
    return parse_char_literal_pp(ctx, ok);
  }

  if (isdigit((unsigned char)*ctx->p)) {
    return parse_number_literal_pp(ctx, ok);
  }

  if (is_ident_start_char(*ctx->p)) {
    while (is_ident_char(*ctx->p))
      ctx->p++;
    return 0;
  }

  *ok = FALSE;
  return 0;
}

// 単項演算子を解析する
static long long parse_unary(ExprParser *ctx, int *ok) {
  skip_spaces_expr(ctx);
  if (*ctx->p == '+') {
    ctx->p++;
    return parse_unary(ctx, ok);
  }
  if (*ctx->p == '-') {
    ctx->p++;
    return -parse_unary(ctx, ok);
  }
  if (*ctx->p == '!') {
    ctx->p++;
    long long val = parse_unary(ctx, ok);
    return *ok ? (long long)(!val) : (long long)0;
  }
  if (*ctx->p == '~') {
    ctx->p++;
    long long val = parse_unary(ctx, ok);
    return *ok ? ~val : (long long)0;
  }
  return parse_primary(ctx, ok);
}

// 乗除剰余演算を解析する
static long long parse_mul(ExprParser *ctx, int *ok) {
  long long val = parse_unary(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (*ctx->p == '*') {
      ctx->p++;
      long long rhs = parse_unary(ctx, ok);
      if (!*ok)
        return 0;
      val *= rhs;
    } else if (*ctx->p == '/') {
      ctx->p++;
      long long rhs = parse_unary(ctx, ok);
      if (!*ok)
        return 0;
      if (rhs == 0)
        error("division by zero in #if expression");
      val /= rhs;
    } else if (*ctx->p == '%') {
      ctx->p++;
      long long rhs = parse_unary(ctx, ok);
      if (!*ok)
        return 0;
      if (rhs == 0)
        error("division by zero in #if expression");
      val %= rhs;
    } else {
      break;
    }
  }
  return val;
}

// 加減算を解析する
static long long parse_add(ExprParser *ctx, int *ok) {
  long long val = parse_mul(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (*ctx->p == '+') {
      ctx->p++;
      long long rhs = parse_mul(ctx, ok);
      if (!*ok)
        return 0;
      val += rhs;
    } else if (*ctx->p == '-') {
      ctx->p++;
      long long rhs = parse_mul(ctx, ok);
      if (!*ok)
        return 0;
      val -= rhs;
    } else {
      break;
    }
  }
  return val;
}

// シフト演算を解析する
static long long parse_shift(ExprParser *ctx, int *ok) {
  long long val = parse_add(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (startswith((char *)ctx->p, "<<")) {
      ctx->p += 2;
      long long rhs = parse_add(ctx, ok);
      if (!*ok)
        return 0;
      val = val << rhs;
    } else if (startswith((char *)ctx->p, ">>")) {
      ctx->p += 2;
      long long rhs = parse_add(ctx, ok);
      if (!*ok)
        return 0;
      val = val >> rhs;
    } else {
      break;
    }
  }
  return val;
}

// 比較演算（<, >, <=, >=）を解析する
static long long parse_relational(ExprParser *ctx, int *ok) {
  long long val = parse_shift(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (startswith((char *)ctx->p, "<=")) {
      ctx->p += 2;
      long long rhs = parse_shift(ctx, ok);
      if (!*ok)
        return 0;
      val = val <= rhs;
    } else if (startswith((char *)ctx->p, ">=")) {
      ctx->p += 2;
      long long rhs = parse_shift(ctx, ok);
      if (!*ok)
        return 0;
      val = val >= rhs;
    } else if (*ctx->p == '<') {
      ctx->p++;
      long long rhs = parse_shift(ctx, ok);
      if (!*ok)
        return 0;
      val = val < rhs;
    } else if (*ctx->p == '>') {
      ctx->p++;
      long long rhs = parse_shift(ctx, ok);
      if (!*ok)
        return 0;
      val = val > rhs;
    } else {
      break;
    }
  }
  return val;
}

// 等価/不等価演算（==, !=）を解析する
static long long parse_equality(ExprParser *ctx, int *ok) {
  long long val = parse_relational(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (startswith((char *)ctx->p, "==")) {
      ctx->p += 2;
      long long rhs = parse_relational(ctx, ok);
      if (!*ok)
        return 0;
      val = val == rhs;
    } else if (startswith((char *)ctx->p, "!=")) {
      ctx->p += 2;
      long long rhs = parse_relational(ctx, ok);
      if (!*ok)
        return 0;
      val = val != rhs;
    } else {
      break;
    }
  }
  return val;
}

// ビット AND 演算を解析する
static long long parse_bitand(ExprParser *ctx, int *ok) {
  long long val = parse_equality(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (*ctx->p == '&' && ctx->p[1] != '&') {
      ctx->p++;
      long long rhs = parse_equality(ctx, ok);
      if (!*ok)
        return 0;
      val = val & rhs;
    } else {
      break;
    }
  }
  return val;
}

// ビット XOR 演算を解析する
static long long parse_bitxor(ExprParser *ctx, int *ok) {
  long long val = parse_bitand(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (*ctx->p == '^') {
      ctx->p++;
      long long rhs = parse_bitand(ctx, ok);
      if (!*ok)
        return 0;
      val = val ^ rhs;
    } else {
      break;
    }
  }
  return val;
}

// ビット OR 演算を解析する
static long long parse_bitor(ExprParser *ctx, int *ok) {
  long long val = parse_bitxor(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (*ctx->p == '|' && ctx->p[1] != '|') {
      ctx->p++;
      long long rhs = parse_bitxor(ctx, ok);
      if (!*ok)
        return 0;
      val = val | rhs;
    } else {
      break;
    }
  }
  return val;
}

// 論理 AND 演算を解析する
static long long parse_logical_and(ExprParser *ctx, int *ok) {
  long long val = parse_bitor(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (startswith((char *)ctx->p, "&&")) {
      ctx->p += 2;
      long long rhs = parse_bitor(ctx, ok);
      if (!*ok)
        return 0;
      val = (val && rhs);
    } else {
      break;
    }
  }
  return val;
}

// 論理 OR 演算を解析する
static long long parse_logical_or(ExprParser *ctx, int *ok) {
  long long val = parse_logical_and(ctx, ok);
  if (!*ok)
    return 0;
  while (TRUE) {
    skip_spaces_expr(ctx);
    if (startswith((char *)ctx->p, "||")) {
      ctx->p += 2;
      long long rhs = parse_logical_and(ctx, ok);
      if (!*ok)
        return 0;
      val = (val || rhs);
    } else {
      break;
    }
  }
  return val;
}

// 三項演算子まで対応する条件式の最終段
static long long parse_conditional(ExprParser *ctx, int *ok) {
  long long cond = parse_logical_or(ctx, ok);
  if (!*ok)
    return 0;
  skip_spaces_expr(ctx);
  if (*ctx->p != '?')
    return cond;
  ctx->p++;
  long long true_val = parse_conditional(ctx, ok);
  if (!*ok)
    return 0;
  skip_spaces_expr(ctx);
  if (*ctx->p != ':') {
    *ok = FALSE;
    return 0;
  }
  ctx->p++;
  long long false_val = parse_conditional(ctx, ok);
  if (!*ok)
    return 0;
  return cond ? true_val : false_val;
}

// 文字列で渡された #if 条件式を評価して真偽値を返す
static int evaluate_if_expression(char *expr_start, char *expr_end, int *result) {
  char *trimmed = copy_trim_directive_expr(expr_start, expr_end);
  if (!trimmed[0]) {
    free(trimmed);
    return FALSE;
  }
  char *expanded = expand_expression_internal(trimmed);
  ExprParser parser;
  parser.p = expanded;
  int ok = TRUE;
  long long value = parse_conditional(&parser, &ok);
  if (ok) {
    skip_spaces_expr(&parser);
    if (*parser.p != '\0')
      ok = FALSE;
  }
  free(trimmed);
  free(expanded);
  if (!ok)
    return FALSE;
  *result = value != 0;
  return TRUE;
}

// #if 行を解析して条件付きコンパイルの状態をプッシュする
int parse_if_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "if") && !is_alnum(cur[2]))) {
    return 0;
  }

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
  } else if (!evaluate_if_expression(expr_start, scan, &cond)) {
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

// #elif 行を解析して次の候補ブランチを判定する
int parse_elif_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "elif") && !is_alnum(cur[4]))) {
    return 0;
  }

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
  if (should_eval && !evaluate_if_expression(expr_start, scan, &cond)) {
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

// #else 行を解析して最後のブランチを切り替える
int parse_else_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "else") && !is_alnum(cur[4]))) {
    return 0;
  }

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

// #endif 行を解析して条件付きコンパイルの状態をポップする
int parse_endif_directive(char **p) {
  char *hash = *p;
  char *cur = hash;
  if (*cur != '#')
    return 0;
  cur++;
  while (*cur == ' ' || *cur == '\t')
    cur++;
  if (!(startswith(cur, "endif") && !is_alnum(cur[5]))) {
    return 0;
  }

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

// 現在、条件付きコンパイルで入力をスキップ中かどうか返す
int preprocess_is_skipping(void) { return skip_depth > 0; }

// 入力終了時に未終了の #if が残っていないか検査する
void preprocess_check_unterminated_ifs(void) {
  if (if_stack) {
    error_at(new_location(if_stack->start), "unterminated #if directive");
  }
}
