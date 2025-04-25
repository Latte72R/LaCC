
#include "lcc.h"

//
// Tokenizer
//

extern char *user_input;
extern char *filename;

// Reports an error and exit.
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "\033[31m");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\033[0m\n");
  exit(1);
}

// エラーの起きた場所を報告するための関数
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  // locが含まれている行の開始地点と終了地点を取得
  char *line = loc;
  while (user_input < line && line[-1] != '\n')
    line--;

  char *end = loc;
  while (*end != '\n') {
    if (*end == '\0') {
      break;
    }
    end++;
  }

  // 見つかった行が全体の何行目なのかを調べる
  int line_num = 1;
  for (char *p = user_input; p < line; p++)
    if (*p == '\n')
      line_num++;

  // 見つかった行を、ファイル名と行番号と一緒に表示
  int indent = fprintf(stderr, "\033[1m\033[32m %s:%d\033[0m: ", filename, line_num) - 13;
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // エラー箇所を"^"で指し示して、エラーメッセージを表示
  int pos = loc - line + indent;
  char fmt2[128];
  sprintf(fmt2, "\033[1m\033[31m^\033[0m %s\n", fmt);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  vfprintf(stderr, fmt2, ap);
  exit(1);
}

// Create a new token and add it as the next token of `cur`.
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

int startswith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}

// Tokenize `user_input` and returns new tokens.
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // Skip whitespace characters.
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 行コメントをスキップ
    if (strncmp(p, "//", 2) == 0) {
      p += 2;
      while (*p != '\n' && *p != '\0')
        p++;
      continue;
    }

    // ブロックコメントをスキップ
    if (strncmp(p, "/*", 2) == 0) {
      char *q = strstr(p + 2, "*/");
      if (!q)
        error_at(p, "unclosed block comment [in tokenize]");
      p = q + 2;
      continue;
    }

    // Multi-letter punctuator
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=") ||
        startswith(p, "&&") || startswith(p, "||") || startswith(p, "++") || startswith(p, "--") ||
        startswith(p, "+=") || startswith(p, "-=") || startswith(p, "*=") || startswith(p, "/=") ||
        startswith(p, "%=") || startswith(p, "->")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    // Single-letter punctuator
    if (strchr("+-*/()<>={}[];&,%%!", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    if (strncmp(p, "sizeof", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "else", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "extern", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_EXTERN, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "enum", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_ENUM, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "struct", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_STRUCT, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "typedef", 7) == 0 && !is_alnum(p[7])) {
      cur = new_token(TK_TYPEDEF, cur, p, 7);
      p += 7;
      continue;
    }

    if (strncmp(p, "if", 2) == 0 && !is_alnum(p[2])) {
      cur = new_token(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }

    if (strncmp(p, "while", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "for", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "int", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_TYPE, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "char", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "void", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "break", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_BREAK, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "continue", 8) == 0 && !is_alnum(p[8])) {
      cur = new_token(TK_CONTINUE, cur, p, 8);
      p += 8;
      continue;
    }

    if (*p == '"') {
      char *q = ++p;
      while (*p != '"') {
        if (*p == '\0') {
          error_at(q - 1, "unclosed string literal [in tokenize]");
        }
        p++;
      }
      cur = new_token(TK_STR, cur, q, p - q);
      p++;
      continue;
    }

    if (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || *p == '_') {
      int i = 0;
      while (('a' <= *(p + i) && *(p + i) <= 'z') || ('A' <= *(p + i) && *(p + i) <= 'Z') ||
             ('0' <= *(p + i) && *(p + i) <= '9') || *(p + i) == '_') {
        i++;
      }
      cur = new_token(TK_IDENT, cur, p, i);
      p += i;
      continue;
    }

    error_at(p, "invalid token [in tokenize]");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}
