
void *NULL = 0;

char *user_input;

typedef enum {
  TK_RESERVED,
  TK_IDENT,
  TK_TYPE,
  TK_NUM,
  TK_RETURN,
  TK_IF,
  TK_WHILE,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_EXTERN,
  TK_STR,
  TK_TYPEDEF,
  TK_ENUM,
  TK_STRUCT,
  TK_EOF
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token *next;    // Next token
  int val;        // If kind is TK_NUM, its value
  char *str;      // Token string
  int len;        // Token length
};

// extention.c
char *read_file();
void init();

// stdio.h
void printf();

// ctype.h
int isspace();
int isdigit();

// string.h
int memcmp();
int strlen();
int strtol();
char *strstr();
char *strchr();

// stdlib.h
void *calloc();
void *realloc();

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
  char *q;
  int i;
  Token head;
  (&head)->next = NULL;
  Token *cur = &head;

  while (*p) {
    // Skip whitespace characters.
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 行コメントをスキップ
    if (startswith(p, "//")) {
      p += 2;
      while (*p != '\n' && *p != '\0')
        p++;
      continue;
    }

    // ブロックコメントをスキップ
    if (startswith(p, "/*")) {
      q = strstr(p + 2, "*/");
      if (!q)
        printf("unclosed block comment [in tokenize]");
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
    if (strchr("+-*/()<>={}[];&,%!", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    // char
    if (*p == '\'') {
      q = p;
      int len;
      p++;
      if (*p == '\\') {
        p++;
        len = 4;
      } else {
        len = 3;
      }
      cur = new_token(TK_NUM, cur, q, len);
      cur->val = *p;
      p++;
      if (*p != '\'') {
        printf("unclosed string literal [in tokenize]");
      }
      p++;
      continue;
    }

    if (*p == '"') {
      p++;
      q = p;
      while (*p != '"') {
        if (*p == '\0') {
          printf("unclosed string literal [in tokenize]");
        }
        if (*p == '\\') {
          p += 2;
        } else {
          p++;
        }
      }
      cur = new_token(TK_STR, cur, q, p - q);
      p++;
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    if (startswith(p, "sizeof") && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "else") && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "return") && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "extern") && !is_alnum(p[6])) {
      cur = new_token(TK_EXTERN, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "enum") && !is_alnum(p[4])) {
      cur = new_token(TK_ENUM, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "struct") && !is_alnum(p[6])) {
      cur = new_token(TK_STRUCT, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "typedef") && !is_alnum(p[7])) {
      cur = new_token(TK_TYPEDEF, cur, p, 7);
      p += 7;
      continue;
    }

    if (startswith(p, "if") && !is_alnum(p[2])) {
      cur = new_token(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }

    if (startswith(p, "while") && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "for") && !is_alnum(p[3])) {
      cur = new_token(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }

    if (startswith(p, "int") && !is_alnum(p[3])) {
      cur = new_token(TK_TYPE, cur, p, 3);
      p += 3;
      continue;
    }

    if (startswith(p, "char") && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "void") && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "break") && !is_alnum(p[5])) {
      cur = new_token(TK_BREAK, cur, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "continue") && !is_alnum(p[8])) {
      cur = new_token(TK_CONTINUE, cur, p, 8);
      p += 8;
      continue;
    }

    if (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || *p == '_') {
      i = 0;
      while (('a' <= *(p + i) && *(p + i) <= 'z') || ('A' <= *(p + i) && *(p + i) <= 'Z') ||
             ('0' <= *(p + i) && *(p + i) <= '9') || *(p + i) == '_') {
        i++;
      }
      cur = new_token(TK_IDENT, cur, p, i);
      p += i;
      continue;
    }

    printf("invalid token [in tokenize]");
  }

  new_token(TK_EOF, cur, p, 0);
  return (&head)->next;
}

int main() {
  user_input = "for (int i = 0; i < 10; i++)";
  for (Token *token = tokenize(); token->kind != TK_EOF; token = token->next) {
    printf("%.*s\n", token->len, token->str);
  }
  return 0;
}
