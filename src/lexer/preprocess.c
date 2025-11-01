
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
}

static void define_macro(char *name, char *body) {
  Macro *macro = find_macro(name, strlen(name));
  if (macro) {
    free_macro_contents(macro);
    macro->name = name;
    macro->body = body;
    macro->file = input_file;
    macro->is_expanding = FALSE;
    return;
  }

  Macro *new_macro = malloc(sizeof(Macro));
  if (!new_macro)
    error("memory allocation failed");
  new_macro->name = name;
  new_macro->body = body;
  new_macro->file = input_file;
  new_macro->is_expanding = FALSE;
  new_macro->next = macros;
  macros = new_macro;
}

void expand_macro(Macro *macro) {
  if (!macro) {
    return;
  }

  if (macro->is_expanding) {
    error("recursive macro expansion: %s", macro->name);
  }

  char *saved_input = user_input;
  char *saved_file = input_file;

  macro->is_expanding = TRUE;
  user_input = macro->body;
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
  while (*cur && isspace(*cur)) {
    if (*cur == '\n') {
      error_at(new_location(cur), "expected an identifier after #define");
    }
    cur++;
  }

  if (!((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z') || *cur == '_')) {
    error_at(new_location(cur), "expected an identifier after #define");
  }

  char *name_start = cur;
  while (('a' <= *cur && *cur <= 'z') || ('A' <= *cur && *cur <= 'Z') || ('0' <= *cur && *cur <= '9') || *cur == '_') {
    cur++;
  }
  int name_len = cur - name_start;
  char *name = malloc(name_len + 1);
  if (!name) {
    error("memory allocation failed");
  }
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  while (*cur && isspace(*cur)) {
    if (*cur == '\n') {
      error_at(new_location(cur), "expected an identifier after #define");
    }
    cur++;
  }

  char *value_start = cur;
  while (*cur && *cur != '\n') {
    cur++;
  }
  char *value_end = cur;
  while (value_end > value_start && value_end[-1] != '\n' && isspace(value_end[-1])) {
    value_end--;
  }

  int value_len = value_end - value_start;
  char *value = malloc(value_len + 2);
  if (!value) {
    error("memory allocation failed");
  }
  if (value_len > 0) {
    memcpy(value, value_start, value_len);
  }
  value[value_len] = '\n';
  value[value_len + 1] = '\0';

  define_macro(name, value);

  if (*cur == '\n') {
    cur++;
  }
  *p = cur;
  return 1;
}
