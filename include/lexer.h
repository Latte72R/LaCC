#ifndef LEXER_H
#define LEXER_H

#include "source.h"

typedef enum {
  TK_EOF,      // 入力の終わりを表すトークン
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_TYPE,     // 型
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_CONST,    // const
  TK_STATIC,   // static
  TK_EXTERN,   // extern
  TK_INLINE,   // inline
  TK_SWITCH,   // switch
  TK_CASE,     // case
  TK_DEFAULT,  // default
  TK_IF,       // if
  TK_ELSE,     // else
  TK_WHILE,    // while
  TK_FOR,      // for
  TK_BREAK,    // break
  TK_CONTINUE, // continue
  TK_DO,       // do
  TK_GOTO,     // goto
  TK_STRING,   // 文字列
  TK_TYPEDEF,  // typedef
  TK_ENUM,     // enum
  TK_UNION,    // union
  TK_STRUCT    // struct
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  Token *next;
  int val;
  int lit_rank;        // 0=int, 1=long, 2=long long
  int lit_is_unsigned; // non-zero if literal has u/U suffix
  char *str;
  int len;
  Location *loc;
};

typedef struct Macro Macro;
struct Macro {
  Macro *next;
  char *name;
  char *body;
  char *file;
  char **params;
  int param_count;
  int is_function;
  int is_variadic;
  int is_expanding;
};

void push_input_context(char *input, char *path, int line_offset);
void pop_input_context();
Location *new_location(char *loc);
void new_token(TokenKind kind, char *loc, char *str, int len);
void tokenize();
void preprocess_initialize_builtins();

#endif // LEXER_H
