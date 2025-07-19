
#include "lacc.h"

extern char *user_input;
extern Token *token;
extern String *filenames;
extern char *filename;
extern IncludePath *include_paths;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

char *find_file_includes(const char *name) {
  char *src = read_file(name);
  if (src)
    return src;

  for (IncludePath *ip = include_paths; ip->next; ip = ip->next) {
    int plen = strlen(ip->path);
    int nlen = strlen(name);
    char *full = malloc(plen + 1 + nlen + 1);
    memcpy(full, ip->path, plen);
    full[plen] = '/';
    memcpy(full + plen + 1, name, nlen + 1);
    src = read_file(full);
    free(full);
    if (src)
      return src;
  }
  return NULL;
}

// Create a new token and add it as the next token of `cur`.
void new_token(TokenKind kind, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  token->next = tok;
  token = tok;
}

int startswith(char *p, char *q) { return !memcmp(p, q, strlen(q)); }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}

// Tokenize `user_input` and returns new tokens.
void tokenize() {
  Token *token_cpy;
  char *p = user_input;
  char *q;

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
        error_at(p, "unclosed block comment [in tokenize]");
      p = q + 2;
      continue;
    }

    // Multi-letter punctuator
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=") ||
        startswith(p, "&&") || startswith(p, "||") || startswith(p, "++") || startswith(p, "--") ||
        startswith(p, "+=") || startswith(p, "-=") || startswith(p, "*=") || startswith(p, "/=") ||
        startswith(p, "%=") || startswith(p, "->") || startswith(p, "<<") || startswith(p, ">>")) {
      new_token(TK_RESERVED, p, 2);
      p += 2;
      continue;
    }

    // Single-letter punctuator
    if (strchr("+-*/()<>={}[];&|^~,%!.:#", *p)) {
      token_cpy = token;
      new_token(TK_RESERVED, p++, 1);
      continue;
    }

    // char
    if (*p == '\'') {
      q = p;
      int len;
      char val;
      p++;

      if (*p == '\\') {
        p++;
        if (*p == 'n') {
          val = '\n';
        } else if (*p == 't') {
          val = '\t';
        } else if (*p == 'r') {
          val = '\r';
        } else if (*p == 'b') {
          val = '\b';
        } else if (*p == 'f') {
          val = '\f';
        } else if (*p == 'v') {
          val = '\v';
        } else if (*p == '\\') {
          val = '\\';
        } else if (*p == '\'') {
          val = '\'';
        } else if (*p == '\"') {
          val = '\"';
        } else if (*p == '0') {
          val = '\0';
        } else {
          error_at(p - 1, "invalid escape sequence in character literal");
        }
        p++;
        len = 4;
      } else {
        val = *p;
        p++;
        len = 3;
      }

      if (*p != '\'') {
        error_at(p, "unclosed character literal");
      }
      p++;

      new_token(TK_NUM, q, len);
      token->val = val;
      continue;
    }

    if (*p == '"') {
      p++;
      q = p;
      while (*p != '"') {
        if (*p == '\0') {
          error_at(q - 1, "unclosed string literal [in tokenize]");
        }
        if (*p == '\\') {
          p += 2;
        } else {
          p++;
        }
      }
      if (token->kind == TK_STRING) {
        memcpy(token->str + token->len, q, p - q);
        token->len += p - q;
      } else {
        new_token(TK_STRING, q, p - q);
      }
      p++;
      continue;
    }

    // Integer literal
    if (startswith(p, "0b")) {
      new_token(TK_NUM, p, 0);
      p += 2;
      q = p;
      token->val = strtol(p, &p, 2);
      token->len = p - q + 2;
      continue;
    } else if (startswith(p, "0x")) {
      p += 2;
      q = p;
      new_token(TK_NUM, p, 0);
      token->val = strtol(p, &p, 16);
      token->len = p - q + 2;
      continue;
    } else if (isdigit(*p)) {
      new_token(TK_NUM, p, 0);
      q = p;
      token->val = strtol(p, &p, 10);
      token->len = p - q;
      continue;
    }

    if (startswith(p, "include") && !is_alnum(p[7])) {
      if (!(token->kind == TK_RESERVED && !memcmp(token->str, "#", token->len))) {
        error_at(token->str, "expected \"#\" before \"include\"");
      }
      token = token_cpy;
      p += 7;
      while (isspace(*p)) {
        p++;
      }
      if (*p != '"') {
        error_at(p, "expected \" before \"include\"");
      }
      p++;
      q = p;
      while (*p != '"') {
        if (*p == '\0') {
          error_at(q - 1, "unclosed string literal [in tokenize]");
        }
        if (*p == '\\') {
          p += 2;
        } else {
          p++;
        }
      }
      char *name = malloc(sizeof(char) * (p - q + 1));
      memcpy(name, q, p - q);
      name[p - q] = '\0';
      p++;
      int flag = 1;
      for (String *s = filenames; s; s = s->next) {
        if (!memcmp(s->text, name, strlen(name))) {
          flag = 0;
          break;
        }
      }
      if (flag == 0) {
        free(name);
        continue;
      }
      filename = name;
      filenames = malloc(sizeof(String));
      filenames->text = filename;
      filenames->len = strlen(filename);
      filenames->next = NULL;
      char *user_input_cpy = user_input;
      char *new_input = find_file_includes(name);
      if (!new_input) {
        error_at(q - 1, "Cannot open include file: %s", name);
      }
      user_input = new_input;
      tokenize();
      user_input = user_input_cpy;
      continue;
    }

    if (startswith(p, "do") && !is_alnum(p[2])) {
      new_token(TK_DO, p, 2);
      p += 2;
      continue;
    }

    if (startswith(p, "if") && !is_alnum(p[2])) {
      new_token(TK_IF, p, 2);
      p += 2;
      continue;
    }

    if (startswith(p, "sizeof") && !is_alnum(p[6])) {
      new_token(TK_SIZEOF, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "else") && !is_alnum(p[4])) {
      new_token(TK_ELSE, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "return") && !is_alnum(p[6])) {
      new_token(TK_RETURN, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "goto") && !is_alnum(p[4])) {
      new_token(TK_GOTO, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "extern") && !is_alnum(p[6])) {
      new_token(TK_EXTERN, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "enum") && !is_alnum(p[4])) {
      new_token(TK_ENUM, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "struct") && !is_alnum(p[6])) {
      new_token(TK_STRUCT, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "typedef") && !is_alnum(p[7])) {
      new_token(TK_TYPEDEF, p, 7);
      p += 7;
      continue;
    }

    if (startswith(p, "const") && !is_alnum(p[5])) {
      new_token(TK_CONST, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "volatile") && !is_alnum(p[8])) {
      // "volatile" is not supported yet
      // Because not performing any optimizations, special handling for volatile isn't necessary.
      p += 8;
      continue;
    }

    if (startswith(p, "static") && !is_alnum(p[6])) {
      new_token(TK_STATIC, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "while") && !is_alnum(p[5])) {
      new_token(TK_WHILE, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "for") && !is_alnum(p[3])) {
      new_token(TK_FOR, p, 3);
      p += 3;
      continue;
    }

    if (startswith(p, "int") && !is_alnum(p[3])) {
      new_token(TK_TYPE, p, 3);
      token->ty = TY_INT;
      p += 3;
      continue;
    }

    if (startswith(p, "char") && !is_alnum(p[4])) {
      new_token(TK_TYPE, p, 4);
      token->ty = TY_CHAR;
      p += 4;
      continue;
    }

    if (startswith(p, "void") && !is_alnum(p[4])) {
      new_token(TK_TYPE, p, 4);
      token->ty = TY_VOID;
      p += 4;
      continue;
    }

    if (startswith(p, "break") && !is_alnum(p[5])) {
      new_token(TK_BREAK, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "continue") && !is_alnum(p[8])) {
      new_token(TK_CONTINUE, p, 8);
      p += 8;
      continue;
    }

    if (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || *p == '_') {
      int i = 0;
      while (('a' <= *(p + i) && *(p + i) <= 'z') || ('A' <= *(p + i) && *(p + i) <= 'Z') ||
             ('0' <= *(p + i) && *(p + i) <= '9') || *(p + i) == '_') {
        i++;
      }
      int j = i;
      while (isspace(*(p + j))) {
        j++;
      }
      if (*(p + j) == ':') {
        new_token(TK_LABEL, p, i);
        j++;
      } else {
        new_token(TK_IDENT, p, i);
      }
      p += j;
      continue;
    }

    error_at(p, "invalid token [in tokenize]");
  }
}
