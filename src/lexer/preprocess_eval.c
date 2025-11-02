#include "lacc.h"

#include <ctype.h>
#include <stdlib.h>

extern const int TRUE;
extern const int FALSE;

// dependencies
extern Macro *find_macro(char *name, int len);
extern char *expand_expression_internal(const char *expr);
extern char *copy_trim_directive_expr(const char *start, const char *end);
extern int is_ident_char(char c);
extern int is_ident_start_char(char c);

typedef struct {
  const char *p;
} ExprParser;

static void skip_spaces_expr(ExprParser *ctx) {
  while (isspace((unsigned char)*ctx->p))
    ctx->p++;
}

static long long parse_conditional(ExprParser *ctx, int *ok);

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
    case '"':
      val = '"';
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

static long long parse_primary(ExprParser *ctx, int *ok) {
  skip_spaces_expr(ctx);
  // Support wide-char literals like L'\0', u'X', U'X' in #if/#elif expressions
  if ((ctx->p[0] == 'L' || ctx->p[0] == 'u' || ctx->p[0] == 'U') && ctx->p[1] == '\'') {
    ctx->p++; // consume prefix and delegate to char literal parser
    return parse_char_literal_pp(ctx, ok);
  }
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
    int ident_len = (int)(ctx->p - ident_start);
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
  if (*ctx->p == '\'')
    return parse_char_literal_pp(ctx, ok);
  if (isdigit((unsigned char)*ctx->p))
    return parse_number_literal_pp(ctx, ok);
  if (is_ident_start_char(*ctx->p)) {
    while (is_ident_char(*ctx->p))
      ctx->p++;
    return 0;
  }
  *ok = FALSE;
  return 0;
}

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
    long long v = parse_unary(ctx, ok);
    return *ok ? (long long)(!v) : 0;
  }
  if (*ctx->p == '~') {
    ctx->p++;
    long long v = parse_unary(ctx, ok);
    return *ok ? ~v : 0;
  }
  return parse_primary(ctx, ok);
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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
    } else
      break;
  }
  return val;
}

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

int preprocess_evaluate_if_expression(char *expr_start, char *expr_end, int *result) {
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
  *result = (value != 0);
  return TRUE;
}
