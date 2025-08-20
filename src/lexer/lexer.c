
#include "lacc.h"

extern char *user_input;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

Location *new_location(char *loc) {
  Location *location = malloc(sizeof(Location));
  location->loc = loc;
  location->path = input_file;
  location->input = user_input;
  return location;
}

// Create a new token and add it as the next token of `cur`.
void new_token(TokenKind kind, char *loc, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  if (len > 0) {
    tok->str = malloc(len + 1);
    strncpy(tok->str, str, len);
    tok->str[len] = '\0';
    register_char_ptr(tok->str);
  } else {
    tok->str = NULL;
  }
  tok->len = len;
  tok->next = NULL;
  tok->loc = new_location(loc);
  if (!token) {
    token_head = tok;
    token = tok;
  } else {
    token->next = tok;
    token = tok;
  }
}

int startswith(char *p, char *q) { return !strncmp(p, q, strlen(q)); }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}

char *handle_include_directive(char *p) {
  char *q;
  p += 8; // "#include" の直後まで進める
  while (isspace(*p)) {
    p++; // 空白を読み飛ばす
  }
  if (*p != '"') {
    error_at(new_location(p), "expected \" before \"include\"");
  }
  p++;   // 先頭の引用符をスキップ
  q = p; // ファイル名の開始位置を記録
  while (*p != '"') {
    if (*p == '\0') {
      error_at(new_location(q - 1), "unclosed string literal [in tokenize]");
    }
    if (*p == '\\') {
      p += 2; // エスケープされた文字を飛ばす
    } else {
      p++;
    }
  }
  // 抽出したファイル名を確保してコピー
  char *name = malloc(sizeof(char) * (p - q + 1));
  memcpy(name, q, p - q);
  name[p - q] = '\0';
  p++; // 閉じる引用符の次へ進める
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
    return p; // 既にインクルード済みならそのまま返す
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
  if (!new_input) {
    error_at(new_location(q - 1), "Cannot open include file: %s", name);
  }
  // 新しいファイルをトークナイズ
  user_input = new_input;
  tokenize();
  // トークナイズ後は元の入力に戻す
  user_input = user_input_prev;
  input_file = input_file_prev;
  free(new_input);
  return p;
}
