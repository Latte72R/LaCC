
#ifndef _LCC_H_
#define _LCC_H_

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

//
// Tokenizer
//

typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_TYPE,     // 型
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_IF,
  TK_WHILE,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_EXTERN,
  TK_STR,     // 文字列
  TK_TYPEDEF, // typedef
  TK_ENUM,    // enum
  TK_STRUCT,  // struct
  TK_EOF,     // 入力の終わりを表すトークン
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

// Reports an error and exit.
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
int consume(char *op);
void expect(char *op, char *err, char *st);
int expect_number();
int at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
int startswith(char *p, char *q);
Token *tokenize();

// ローカル変数の型

typedef struct String String;
struct String {
  String *next;
  char *text;
  int len;
  int label;
};

typedef enum { TY_INT, TY_CHAR, TY_PTR, TY_ARR, TY_VOID, TY_STRUCT, TY_ENUM } TypeKind;

typedef struct Type Type;

typedef struct LVar LVar;
struct LVar {
  LVar *next; // 次の変数かNULL
  char *name; // 変数の名前
  int len;    // 名前の長さ
  int offset; // RBPからのオフセット
  Type *type; // 変数の型
};

typedef struct Struct Struct;
struct Struct {
  Struct *next; // 次の構造体かNULL
  LVar *var;    // 次の変数かNULL
  char *name;   // 変数の名前
  int len;      // 名前の長さ
  int size;     // 構造体のサイズ
};

typedef struct StructTag StructTag;
struct StructTag {
  StructTag *next; // 次の構造体かNULL
  Struct *main;    // struct
  char *name;      // タグの名前
  int len;         // 名前の長さ
};

struct Type {
  TypeKind ty;
  Type *ptr_to;
  int array_size;
  Struct *struct_;
};

// 関数の型
typedef struct Function Function;
struct Function {
  Function *next; // 次の関数かNULL
  LVar *locals;   // ローカル変数
  char *name;     // 変数の名前
  int len;        // 名前の長さ
  Type *type;     // 関数の型
};

//
// Parser
//

typedef enum {
  ND_ADD,      // +
  ND_SUB,      // -
  ND_MUL,      // *
  ND_DIV,      // /
  ND_MOD,      // %
  ND_EQ,       // ==
  ND_NE,       // !=
  ND_LT,       // <
  ND_LE,       // <=
  ND_AND,      // &&
  ND_OR,       // ||
  ND_NOT,      // !
  ND_ASSIGN,   // =
  ND_LVAR,     // ローカル変数
  ND_VARDEC,   // 変数宣言
  ND_GVAR,     // グローバル変数
  ND_GLBDEC,   // グローバル変数宣言
  ND_NUM,      // 整数
  ND_STR,      // 文字列
  ND_ADDR,     // &
  ND_DEREF,    // *
  ND_IF,       // if
  ND_ELSE,     // else
  ND_WHILE,    // while
  ND_FOR,      // for
  ND_BREAK,    // break
  ND_CONTINUE, // continue
  ND_RETURN,   // return
  ND_FUNCDEF,  // 関数定義
  ND_FUNCALL,  // 関数呼び出し
  ND_EXTERN,   // extern
  ND_BLOCK,    // { ... }
  ND_ARR,      // 配列
  ND_ENUM,     // 列挙体
  ND_STRUCT,   // 構造体
  ND_TYPEDEF,  // typedef
  ND_TYPE,     // 型
  ND_NONE,     // 空のノード
} NodeKind;

// 抽象構文木のノード
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型
  Node *lhs;     // 左辺
  Node *rhs;     // 右辺
  int val;       // kindがND_NUMの場合はその数値
  int id;        // kindがND_IF, ND_WHILE, ND_FORの場合のみ使う
  int endline;
  Node *cond;    // kindがND_IF, ND_WHILE, ND_FORの場合のみ使う
  Node *then;    // kindがND_IFの場合のみ使う
  Node *els;     // kindがND_IFの場合のみ使う
  Node *init;    // kindがND_FORの場合のみ使う
  Node *inc;     // kindがND_FORの場合のみ使う
  Node *body;    // kindがND_BLOCKの場合のみ使う
  Node *args[4]; // kindがND_FUNCALLの場合のみ使う
  Function *fn;  // kindがND_FUNCDEF, ND_FUNCALLの場合のみ使う
  LVar *var;     // kindがND_LVAR, ND_GVARの場合のみ使う
  String *str;   // kindがND_STRの場合のみ使う
  Type *type;
};

Node *new_node(NodeKind kind);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
Node *new_num(int val);
int get_type_size(Type *type);

Node *stmt();
Node *assign();
Node *expr();
Node *logical();
Node *equality();
Node *relational();
Node *new_add(Node *lhs, Node *rhs, char *consumed_ptr);
Node *new_sub(Node *lhs, Node *rhs, char *consumed_ptr);
Node *add();
Node *mul();
Node *refer_struct();
Node *unary();
Node *primary();

void program();

void gen(Node *node);

#endif
