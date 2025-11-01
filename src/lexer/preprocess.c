
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

// 文字列から #include 行を解析して処理する
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

// 文字列から #define 行を解析し、マクロを登録する
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
        if (*after_name != '(') {
          append_text(&buf, &len, &cap, start, ident_len);
          continue;
        }
        const char *arg_ptr = after_name;
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
  char *trimmed = copy_trim(expr_start, expr_end);
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
  if (!(startswith(*p, "#if") && !is_alnum((*p)[3]))) {
    return 0;
  }

  char *cur = *p;
  char *expr_start = cur + 3;
  while (*expr_start == ' ' || *expr_start == '\t')
    expr_start++;
  char *scan = expr_start;
  while (*scan && *scan != '\n')
    scan++;

  int ignoring = skip_depth > 0;
  int cond = FALSE;
  if (ignoring) {
    char *trimmed = copy_trim(expr_start, scan);
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
  state->start = cur;
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
  if (!(startswith(*p, "#elif") && !is_alnum((*p)[5]))) {
    return 0;
  }

  if (!if_stack)
    error_at(new_location(*p), "#elif without matching #if");

  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(*p), "#elif after #else");

  char *cur = *p;
  char *expr_start = cur + 5;
  while (*expr_start == ' ' || *expr_start == '\t')
    expr_start++;
  char *scan = expr_start;
  while (*scan && *scan != '\n')
    scan++;

  int should_eval = !state->ignoring && !state->branch_taken;
  int cond = FALSE;
  if (should_eval && !evaluate_if_expression(expr_start, scan, &cond)) {
    error_at(new_location(expr_start), "invalid expression in #elif directive");
  } else if (!should_eval) {
    char *trimmed = copy_trim(expr_start, scan);
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
        error_at(new_location(cur), "internal error: skip depth underflow");
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
  if (!(startswith(*p, "#else") && !is_alnum((*p)[5]))) {
    return 0;
  }

  if (!if_stack)
    error_at(new_location(*p), "#else without matching #if");

  IfState *state = if_stack;
  if (state->else_seen)
    error_at(new_location(*p), "multiple #else directives");

  char *cur = *p;
  char *rest = cur + 5;
  while (*rest && *rest != '\n')
    rest++;
  char *extra = copy_trim(cur + 5, rest);
  if (extra[0]) {
    free(extra);
    error_at(new_location(cur + 5), "unexpected tokens after #else");
  }
  free(extra);

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
        error_at(new_location(cur), "internal error: skip depth underflow");
      skip_depth--;
    }
  }

  if (*rest == '\n')
    rest++;
  *p = rest;
  return 1;
}

// #endif 行を解析して条件付きコンパイルの状態をポップする
int parse_endif_directive(char **p) {
  if (!(startswith(*p, "#endif") && !is_alnum((*p)[6]))) {
    return 0;
  }

  if (!if_stack)
    error_at(new_location(*p), "#endif without matching #if");

  IfState *state = if_stack;
  if_stack = state->prev;

  if (state->ignoring || !state->currently_active) {
    if (skip_depth <= 0)
      error_at(new_location(*p), "internal error: skip depth underflow");
    skip_depth--;
  }

  free(state);

  char *cur = *p + 6;
  while (*cur && *cur != '\n')
    cur++;
  if (*cur == '\n')
    cur++;
  *p = cur;
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
