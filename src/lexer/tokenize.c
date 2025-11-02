
#include "lacc.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern char *user_input;
extern Token *token;

extern const int TRUE;
extern const int FALSE;

static int tokenize_depth = 0;

// マクロ引数解析は preprocess 側の parse_macro_arguments を利用する

static int skip_characters(char **p) {
  char *cur = *p;

  if (isspace(*cur)) {
    cur++;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "//")) {
    cur += 2;
    while (*cur != '\n' && *cur != '\0')
      cur++;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "/*")) {
    char *q = strstr(cur + 2, "*/");
    if (!q)
      error_at(new_location(cur), "unclosed block comment [in tokenize]");
    cur = q + 2;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_char_literal(char **p) {
  char *cur = *p;

  if (*cur != '\'') {
    return 0;
  }

  char *start = cur;
  int len;
  char val;
  cur++;

  if (*cur == '\\') {
    cur++;
    switch (*cur) {
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
      error_at(new_location(cur - 1), "invalid escape sequence in character literal");
    }
    cur++;
    len = 4;
  } else {
    val = *cur;
    cur++;
    len = 3;
  }

  if (*cur != '\'') {
    error_at(new_location(cur), "unclosed character literal");
  }
  cur++;

  new_token(TK_NUM, start, start, len);
  token->val = val;
  *p = cur;
  return 1;
}

static int parse_number_literal(char **p) {
  char *cur = *p;
  if (!isdigit(*cur)) {
    return 0;
  }
  char *digits;
  char *end;
  char *start = cur;
  int base = 10;

  if (startswith(cur, "0b")) {
    cur += 2;
    digits = cur;
    new_token(TK_NUM, start, start, 0);
    base = 2;
    token->val = strtol(cur, &end, base);
    cur = end;
  } else if (startswith(cur, "0x")) {
    cur += 2;
    digits = cur;
    new_token(TK_NUM, start, start, 0);
    base = 16;
    token->val = strtol(cur, &end, base);
    cur = end;
  } else if (startswith(cur, "0")) {
    cur += 1;
    digits = cur;
    new_token(TK_NUM, start, start, 0);
    base = 8;
    token->val = strtol(cur, &end, base);
    cur = end;
  } else {
    digits = cur;
    new_token(TK_NUM, start, start, 0);
    base = 10;
    token->val = strtol(cur, &end, base);
    cur = end;
  }

  // Parse integer literal suffixes: u/U, l/L, ll/LL (in any order)
  int seen_u = 0;
  int l_count = 0;
  for (;;) {
    if (*cur == 'u' || *cur == 'U') {
      seen_u = 1;
      cur++;
      continue;
    }
    if (*cur == 'l' || *cur == 'L') {
      // Count consecutive l/L occurrences (across the whole suffix, not only contiguous)
      l_count++;
      cur++;
      // Support both "ll" and separated sequences like "lL"
      continue;
    }
    break;
  }
  if (l_count >= 2)
    token->lit_rank = 2; // long long
  else if (l_count == 1)
    token->lit_rank = 1; // long
  else
    token->lit_rank = 0; // int
  token->lit_is_unsigned = seen_u;

  // Set token length to cover the whole literal including prefix and suffix
  token->len = cur - start;
  *p = cur;
  return 1;
}

static int parse_string_literal(char **p) {
  char *cur = *p;

  if (*cur != '"') {
    return 0;
  }
  cur++;
  char *literal_start = cur;
  int len = 0;
  int cap = 16;
  char *buf = malloc(sizeof(char) * cap);
  if (!buf)
    error("memory allocation failed");
  while (*cur != '"') {
    switch (*cur) {
    case '\0':
    case '\n':
      error_at(new_location(cur), "unclosed string literal [in tokenize]");
      break;
    case '\\':
      switch (*(cur + 1)) {
      case '\n':
        cur += 2;
        break;
      case 'a':
        error_at(new_location(cur), "unsupported escape sequence \"\\a\" in string literal. Use '\\007' for alert.");
        break;
      case 'e':
        error_at(new_location(cur), "unsupported escape sequence \"\\e\" in string literal. Use '\\033' for escape.");
        break;
      case 'v':
        error_at(new_location(cur),
                 "unsupported escape sequence \"\\v\" in string literal. Use '\\012' for vertical tab.");
        break;
      default:
        buf = safe_realloc_array(buf, sizeof(char), len + 2, &cap);
        buf[len++] = *cur++;
        buf[len++] = *cur++;
        break;
      }
      break;
    default:
      buf = safe_realloc_array(buf, sizeof(char), len + 1, &cap);
      buf[len++] = *cur++;
    }
  }
  buf = safe_realloc_array(buf, sizeof(char), len + 1, &cap);
  buf[len] = '\0';
  if (token && token->kind == TK_STRING) {
    char *old = token->str;
    token->str = realloc(token->str, token->len + len + 1);
    if (!token->str)
      error("memory allocation failed");
    update_char_ptr(old, token->str);
    memcpy(token->str + token->len, buf, len);
    token->len += len;
  } else {
    new_token(TK_STRING, literal_start, buf, len);
  }
  free(buf);
  cur++;
  *p = cur;
  return 1;
}

static int parse_punctuator(char **p) {
  char *cur = *p;

  if (startswith(cur, "...")) {
    new_token(TK_RESERVED, cur, cur, 3);
    cur += 3;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "==") || startswith(cur, "!=") || startswith(cur, "<=") || startswith(cur, ">=") ||
      startswith(cur, "&&") || startswith(cur, "||") || startswith(cur, "++") || startswith(cur, "--") ||
      startswith(cur, "+=") || startswith(cur, "-=") || startswith(cur, "*=") || startswith(cur, "/=") ||
      startswith(cur, "%=") || startswith(cur, "->") || startswith(cur, "<<") || startswith(cur, ">>")) {
    new_token(TK_RESERVED, cur, cur, 2);
    cur += 2;
    *p = cur;
    return 1;
  }

  if (*cur && strchr("+-*/()<>={}[];&|^~,%!.:?", *cur)) {
    new_token(TK_RESERVED, cur, cur, 1);
    cur++;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "sizeof") && !is_alnum(cur[6])) {
    new_token(TK_RESERVED, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_control_structure(char **p) {
  char *cur = *p;

  if (startswith(cur, "if") && !is_alnum(cur[2])) {
    new_token(TK_IF, cur, cur, 2);
    cur += 2;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "else") && !is_alnum(cur[4])) {
    new_token(TK_ELSE, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "switch") && !is_alnum(cur[6])) {
    new_token(TK_SWITCH, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "case") && !is_alnum(cur[4])) {
    new_token(TK_CASE, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "default") && !is_alnum(cur[7])) {
    new_token(TK_DEFAULT, cur, cur, 7);
    cur += 7;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "do") && !is_alnum(cur[2])) {
    new_token(TK_DO, cur, cur, 2);
    cur += 2;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "while") && !is_alnum(cur[5])) {
    new_token(TK_WHILE, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "for") && !is_alnum(cur[3])) {
    new_token(TK_FOR, cur, cur, 3);
    cur += 3;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "break") && !is_alnum(cur[5])) {
    new_token(TK_BREAK, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "continue") && !is_alnum(cur[8])) {
    new_token(TK_CONTINUE, cur, cur, 8);
    cur += 8;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "goto") && !is_alnum(cur[4])) {
    new_token(TK_GOTO, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "return") && !is_alnum(cur[6])) {
    new_token(TK_RETURN, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_declaration_specifier(char **p) {
  char *cur = *p;

  if (startswith(cur, "typedef") && !is_alnum(cur[7])) {
    new_token(TK_TYPEDEF, cur, cur, 7);
    cur += 7;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "const") && !is_alnum(cur[5])) {
    new_token(TK_CONST, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "static") && !is_alnum(cur[6])) {
    new_token(TK_STATIC, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "extern") && !is_alnum(cur[6])) {
    new_token(TK_EXTERN, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "volatile") && !is_alnum(cur[8])) {
    // "volatile" is not supported yet
    // Because not performing any optimizations, special handling for volatile isn't necessary.
    cur += 8;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_composite_type(char **p) {
  char *cur = *p;

  if (startswith(cur, "enum") && !is_alnum(cur[4])) {
    new_token(TK_ENUM, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "union") && !is_alnum(cur[5])) {
    new_token(TK_UNION, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "struct") && !is_alnum(cur[6])) {
    new_token(TK_STRUCT, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_basic_type(char **p) {
  char *cur = *p;

  if (startswith(cur, "signed") && !is_alnum(cur[6])) {
    new_token(TK_TYPE, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "unsigned") && !is_alnum(cur[8])) {
    new_token(TK_TYPE, cur, cur, 8);
    cur += 8;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "long") && !is_alnum(cur[4])) {
    new_token(TK_TYPE, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "short") && !is_alnum(cur[5])) {
    new_token(TK_TYPE, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "int") && !is_alnum(cur[3])) {
    new_token(TK_TYPE, cur, cur, 3);
    cur += 3;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "char") && !is_alnum(cur[4])) {
    new_token(TK_TYPE, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "float") && !is_alnum(cur[5])) {
    new_token(TK_TYPE, cur, cur, 5);
    cur += 5;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "double") && !is_alnum(cur[6])) {
    new_token(TK_TYPE, cur, cur, 6);
    cur += 6;
    *p = cur;
    return 1;
  }

  if (startswith(cur, "void") && !is_alnum(cur[4])) {
    new_token(TK_TYPE, cur, cur, 4);
    cur += 4;
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

static int parse_identifier(char **p) {
  char *cur = *p;

  if (('a' <= *cur && *cur <= 'z') || ('A' <= *cur && *cur <= 'Z') || *cur == '_') {
    char *start = cur;
    while (('a' <= *cur && *cur <= 'z') || ('A' <= *cur && *cur <= 'Z') || ('0' <= *cur && *cur <= '9') ||
           *cur == '_') {
      cur++;
    }
    char *after_name = cur;
    while (isspace((unsigned char)*cur)) {
      cur++;
    }

    int name_len = after_name - start;
    Macro *macro = find_macro(start, name_len);
    if (macro && macro->is_function && !macro->is_expanding) {
      if (*cur == '(') {
        const char *pos = cur;
        int arg_cnt = 0;
        char **args = parse_macro_arguments(&pos, macro, &arg_cnt);
        while (isspace((unsigned char)*pos))
          pos++;
        *p = (char *)pos;
        expand_macro(macro, args, arg_cnt);
        if (args) {
          for (int i = 0; i < arg_cnt; i++)
            free(args[i]);
          free(args);
        }
        return 1;
      } else {
        // 関数型マクロだが直後に '(' がない場合は展開しない
      }
    } else if (macro && !macro->is_expanding) {
      *p = cur;
      expand_macro(macro, NULL, 0);
      return 1;
    }

    new_token(TK_IDENT, start, start, name_len);
    *p = cur;
    return 1;
  }

  *p = cur;
  return 0;
}

// Tokenize `user_input` and returns new tokens.
void tokenize() {
  tokenize_depth++;
  char *p = user_input;

  while (*p) {
    if (skip_characters(&p)) {
      continue;
    }

    if (parse_if_directive(&p)) {
      continue;
    }

    if (parse_ifdef_directive(&p)) {
      continue;
    }

    if (parse_ifndef_directive(&p)) {
      continue;
    }

    if (parse_elif_directive(&p)) {
      continue;
    }

    if (parse_else_directive(&p)) {
      continue;
    }

    if (parse_endif_directive(&p)) {
      continue;
    }

    if (preprocess_is_skipping()) {
      if (*p == '\n') {
        p++;
        continue;
      }
      if (!*p)
        break;
      p++;
      continue;
    }

    if (parse_error_directive(&p)) {
      continue;
    }

    if (parse_warning_directive(&p)) {
      continue;
    }

    if (parse_include_directive(&p)) {
      continue;
    }

    if (parse_define_directive(&p)) {
      continue;
    }

    if (parse_undef_directive(&p)) {
      continue;
    }

    if (parse_pragma_directive(&p)) {
      continue;
    }

    if (parse_punctuator(&p)) {
      continue;
    }

    if (parse_char_literal(&p)) {
      continue;
    }

    if (parse_string_literal(&p)) {
      continue;
    }

    if (parse_number_literal(&p)) {
      continue;
    }

    if (parse_control_structure(&p)) {
      continue;
    }

    if (parse_declaration_specifier(&p)) {
      continue;
    }

    if (parse_composite_type(&p)) {
      continue;
    }

    if (parse_basic_type(&p)) {
      continue;
    }

    if (parse_identifier(&p)) {
      continue;
    }

    error_at(new_location(p), "invalid token [in tokenize]");
  }
  tokenize_depth--;
  if (!tokenize_depth) {
    preprocess_check_unterminated_ifs();
  }
}
