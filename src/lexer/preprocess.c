
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

Macro *find_macro(char *name, int len) {
  for (Macro *macro = macros; macro; macro = macro->next) {
    if ((int)strlen(macro->name) == len && !strncmp(macro->name, name, len)) {
      return macro;
    }
  }
  return NULL;
}

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
    *pending_concat = FALSE;
  }
  append_text(buf, len, cap, text, text_len);
}

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

void handle_include_directive(char *name, char *p) {
  int already_included = FALSE;
  for (FileName *s = filenames; s; s = s->next) {
    if (!strcmp(s->name, name)) {
      // 同じファイルを二重に取り込まない
      already_included = TRUE;
      break;
    }
  }

  if (already_included) {
    free(name);
    return;
  }

  char *input_file_prev = input_file;
  input_file = name; // 現在処理中のファイル名を更新
  // filenames に現在のファイルを追加
  FileName *filename = malloc(sizeof(FileName));
  filename->name = input_file;
  filename->next = filenames;
  filenames = filename;
  char *user_input_prev = user_input;
  char *new_input = read_include_file(name); // ファイル内容を取得
  CharPtrList *user_input_list_prev = user_input_list;
  user_input_list = malloc(sizeof(CharPtrList));
  user_input_list->str = new_input;
  user_input_list->next = user_input_list_prev;
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open include file: %s", name);
  }
  // 新しいファイルをトークナイズ
  user_input = new_input;
  tokenize();
  // トークナイズ後は元の入力に戻す
  user_input = user_input_prev;
  input_file = input_file_prev;
}

int parse_include_directive(char **p) {
  if (!(startswith(*p, "#include") && !is_alnum((*p)[8]))) {
    return 0;
  }

  char *q;
  *p += 8; // "#include" の直後まで進める
  while (isspace(**p)) {
    (*p)++; // 空白を読み飛ばす
  }
  if (**p != '"') {
    error_at(new_location(*p), "expected \" before \"include\"");
  }
  (*p)++; // 先頭の引用符をスキップ
  q = *p; // ファイル名の開始位置を記録
  while (**p != '"') {
    if (**p == '\0') {
      error_at(new_location(q - 1), "unclosed string literal [in tokenize]");
    }
    if (**p == '\\') {
      (*p) += 2; // エスケープされた文字を飛ばす
    } else {
      (*p)++;
    }
  }

  // 抽出したファイル名を確保してコピー
  char *name = malloc(sizeof(char) * ((*p) - q + 1));
  memcpy(name, q, (*p) - q);
  name[(*p) - q] = '\0';
  (*p)++; // 閉じる引用符の次へ進める
  handle_include_directive(name, q);
  return 1;
}

int parse_define_directive(char **p) {
  if (!(startswith(*p, "#define") && !is_alnum((*p)[7]))) {
    return 0;
  }

  char *cur = *p + 7;
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

  char *value_start = cur;
  while (*cur && *cur != '\n') {
    cur++;
  }
  char *value_end = cur;
  while (value_end > value_start && value_end[-1] != '\n' && isspace((unsigned char)value_end[-1])) {
    value_end--;
  }

  int value_len = value_end - value_start;
  char *value = malloc(value_len + 2);
  if (!value)
    error("memory allocation failed");
  if (value_len > 0) {
    memcpy(value, value_start, value_len);
  }
  value[value_len] = '\n';
  value[value_len + 1] = '\0';

  define_macro(name, value, params, param_count, is_function);

  if (*cur == '\n') {
    cur++;
  }
  *p = cur;
  return 1;
}
