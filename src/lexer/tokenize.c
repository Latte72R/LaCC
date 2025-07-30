
#include "lacc.h"

extern char *user_input;
extern Token *token;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

char *parse_char_literal(char *p) {
  char *q = p;
  int len;
  char val;
  p++;

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
  return p; // Return the position after the closing quote
}

char *parse_string_literal(char *p) {
  p++;
  char *q = p;
  int len = 0;
  char *buf = malloc(sizeof(char) * 1024);
  while (*p != '"') {
    if (*p == '\0' || *p == '\n') {
      error_at(p, "unclosed string literal [in tokenize]");
    } else if (*p == '\\' && *(p + 1) == '\n') {
      p += 2; // 行継続をスキップ
    } else if (*p == '\\') {
      buf[len++] = *p++;
      buf[len++] = *p++;
    } else {
      buf[len++] = *p++;
    }
  }
  if (token->kind == TK_STRING) {
    memcpy(token->str + token->len, buf, len);
    token->len += len;
  } else {
    memcpy(q, buf, len);
    new_token(TK_STRING, q, len);
  }
  free(buf);
  p++;
  return p; // Return the position after the closing quote
}

char *parse_number_literal(char *p) {
  char *q;
  if (startswith(p, "0b")) {
    new_token(TK_NUM, p, 0);
    p += 2;
    q = p;
    token->val = strtol(p, &p, 2);
    token->len = p - q + 2;
  } else if (startswith(p, "0x")) {
    p += 2;
    q = p;
    new_token(TK_NUM, p, 0);
    token->val = strtol(p, &p, 16);
    token->len = p - q + 2;
  } else if (startswith(p, "0")) {
    new_token(TK_NUM, p, 0);
    p++;
    q = p;
    token->val = strtol(p, &p, 8);
    token->len = p - q + 2;
  } else {
    new_token(TK_NUM, p, 0);
    q = p;
    token->val = strtol(p, &p, 10);
    token->len = p - q;
  }
  return p; // Return the position after the number
}

// Tokenize `user_input` and returns new tokens.
void tokenize() {
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

    if (startswith(p, "#include") && !is_alnum(p[8])) {
      p = handle_include_directive(p);
      continue;
    }

    // Multi-letter punctuator
    if (startswith(p, "...")) {
      new_token(TK_RESERVED, p, 3);
      p += 3;
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
    if (strchr("+-*/()<>={}[];&|^~,%!.:", *p)) {
      new_token(TK_RESERVED, p++, 1);
      continue;
    }

    // char
    if (*p == '\'') {
      p = parse_char_literal(p);
      continue;
    }

    if (*p == '"') {
      p = parse_string_literal(p);
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      p = parse_number_literal(p);
      continue;
    }

    if (startswith(p, "switch") && !is_alnum(p[6])) {
      new_token(TK_SWITCH, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "case") && !is_alnum(p[4])) {
      new_token(TK_CASE, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "default") && !is_alnum(p[7])) {
      new_token(TK_DEFAULT, p, 7);
      p += 7;
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
      new_token(TK_IDENT, p, i);
      p += j;
      continue;
    }

    error_at(p, "invalid token [in tokenize]");
  }
}
