// lacc.h

//
// Main
//

typedef struct IncludePath IncludePath;
struct IncludePath {
  char *path;
  IncludePath *next;
};

//
// Tokenizer
//

typedef enum {
  TK_EOF,      // 入力の終わりを表すトークン
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_TYPE,     // 型
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_SIZEOF,   // sizeof
  TK_CONST,    // const
  TK_STATIC,   // static
  TK_EXTERN,   // extern
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_DO,
  TK_GOTO,    // GOTO
  TK_LABEL,   // ラベル
  TK_STRING,  // 文字列
  TK_TYPEDEF, // typedef
  TK_ENUM,    // enum
  TK_STRUCT   // struct
} TokenKind;

// ローカル変数の型

typedef struct String String;
struct String {
  String *next;
  char *text;
  int len;
  int id;
};

typedef struct Array Array;
struct Array {
  Array *next;
  int *val;
  int byte;
  int len;
  int id;
};

typedef enum { TY_NONE, TY_INT, TY_CHAR, TY_PTR, TY_ARR, TY_ARGARR, TY_VOID, TY_STRUCT } TypeKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token *next;    // Next token
  int val;        // If kind is TK_NUM, its value
  char *str;      // Token string
  int len;        // Token length
  TypeKind ty;    // Token type
};

typedef struct Type Type;

typedef struct LVar LVar;
struct LVar {
  LVar *next;    // 次の変数かNULL
  char *name;    // 変数の名前
  int len;       // 名前の長さ
  int offset;    // RBPからのオフセット
  int ext;       // externかどうか
  Type *type;    // 変数の型
  int is_static; // staticかどうか
  int block;     // ブロックのID
};

typedef struct Struct Struct;
struct Struct {
  Struct *next; // 次の構造体かNULL
  LVar *var;    // 次の変数かNULL
  char *name;   // 変数の名前
  int len;      // 名前の長さ
  int size;     // 構造体のサイズ
};

typedef struct Enum Enum;
struct Enum {
  Enum *next; // 次のEnumかNULL
  char *name; // 変数の名前
  int len;    // 名前の長さ
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
  int const_; // constかどうか
};

// Label
typedef struct Label Label;
struct Label {
  Label *next; // 次のラベルかNULL
  char *name;  // ラベルの名前
  int len;     // 名前の長さ
  int id;      // ラベルのID
};

// 関数の型
typedef struct Function Function;
struct Function {
  Function *next; // 次の関数かNULL
  LVar *locals;   // ローカル変数
  char *name;     // 変数の名前
  int len;        // 名前の長さ
  int offset;     // RBPからのオフセット
  Type *type;     // 関数の型
  int is_static;  // staticかどうか
  Label *labels;  // ラベルのリスト
};

//
// Parser
//

typedef enum {
  ND_NONE,     // 空のノード
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
  ND_BITNOT,   // ~
  ND_BITAND,   // &
  ND_BITOR,    // |
  ND_BITXOR,   // ^
  ND_SHL,      // <<
  ND_SHR,      // >>
  ND_ASSIGN,   // =
  ND_POSTINC,  // ++ or --
  ND_LVAR,     // ローカル変数
  ND_VARDEC,   // 変数宣言
  ND_GVAR,     // グローバル変数
  ND_GLBDEC,   // グローバル変数宣言
  ND_NUM,      // 整数
  ND_STRING,   // 文字列
  ND_ARRAY,    // 配列
  ND_ADDR,     // &
  ND_DEREF,    // *
  ND_IF,       // if
  ND_ELSE,     // else
  ND_WHILE,    // while
  ND_FOR,      // for
  ND_BREAK,    // break
  ND_CONTINUE, // continue
  ND_DOWHILE,  // do-while
  ND_GOTO,     // goto
  ND_LABEL,    // ラベル
  ND_RETURN,   // return
  ND_FUNCDEF,  // 関数定義
  ND_FUNCALL,  // 関数呼び出し
  ND_EXTERN,   // extern
  ND_BLOCK,    // { ... }
  ND_ENUM,     // 列挙体
  ND_STRUCT,   // 構造体
  ND_TYPEDEF,  // typedef
  ND_TYPE      // 型
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
  Node *step;    // kindがND_FORの場合のみ使う
  Node **body;   // kindがND_BLOCKの場合のみ使う
  Node *args[6]; // kindがND_FUNCALLの場合のみ使う
  Function *fn;  // kindがND_FUNCDEF, ND_FUNCALLの場合のみ使う
  LVar *var;     // kindがND_LVAR, ND_GVARの場合のみ使う
  Type *type;
  Label *label;
};

Node *stmt();
Node *assign_sub(Node *lhs, Node *rhs, char *ptr);
Node *assign();
Node *expr();
Node *equality();
Node *relational();
Node *new_add(Node *lhs, Node *rhs, char *consumed_ptr);
Node *new_sub(Node *lhs, Node *rhs, char *consumed_ptr);
Node *add();
Node *mul();
Node *logical_and();
Node *logical_or();
Node *bit_or();
Node *bit_and();
Node *bit_xor();
Node *bit_shift();
Node *increment_decrement();
Node *access_member();
Node *unary();
Node *primary();
int get_type_size(Type *type);
int get_sizeof(Type *type);

Type *new_type(TypeKind ty);
Type *new_type_ptr(Type *ptr_to);
int is_ptr_or_arr(Type *type);

int startswith(char *p, char *q);

void tokenize();
void new_token(TokenKind kind, char *str, int len);

void program();

void gen(Node *node);

// extention.c
void error();
void error_at();
void warning_at();
char *read_file();
void init();

// stdio.h
void printf();

// ctype.h
int isspace();
int isdigit();

// string.h
int memcmp();
int memcpy();
int strlen();
int strtol();
char *strstr();
char *strchr();

// stdlib.h
void *malloc();
void *realloc();
void free();
