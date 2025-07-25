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
  TK_ELLIPSIS, // ...
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
  int is_extern; // externかどうか
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
  Struct *is_struct;
  int is_const; // constかどうか
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

// tokenize.c
void tokenize();
void new_token(TokenKind kind, char *str, int len);
int startswith(char *p, char *q);

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

// symbol.c
LVar *find_lvar(Token *tok);
LVar *find_gvar(Token *tok);
Struct *find_struct(Token *tok);
Enum *find_enum(Token *tok);
LVar *find_enum_member(Token *tok);
LVar *find_struct_member(Struct *struct_, Token *tok);
StructTag *find_struct_tag(Token *tok);
Function *find_fn(Token *tok);

// token.c
int peek(char *op);
int consume(char *op);
Token *consume_ident();
Token *expect_ident(const char *context);
void expect(char *op, char *err, char *st);
int expect_number();
int parse_sign();
int expect_signed_number();

// type.c
Type *parse_base_type_internal(int should_consume);
Type *check_base_type();
Type *parse_pointer_qualifiers(Type *base_type);
Type *consume_type();
int is_type(Token *tok);
int is_ptr_or_arr(Type *type);
int is_number(Type *type);
int get_sizeof(Type *type);
Type *new_type(TypeKind ty);
Type *new_type_ptr(Type *ptr_to);
Type *new_type_arr(Type *ptr_to, int array_size);
Type *new_type_struct(Struct *struct_);
Type *parse_array_dimensions(Type *base_type);

// ast.c
Node *new_node(NodeKind kind);
Node *new_num(int val);
Node *new_deref(Node *lhs);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
LVar *new_lvar(Token *tok, Type *type, int is_static, int is_extern);

// decl.c
Node *function_definition(Token *tok, Type *type, int is_static);
Node *local_variable_declaration(Token *tok, Type *type, int is_static);
Node *global_variable_declaration(Token *tok, Type *type, int is_static);
Node *extern_variable_declaration(Token *tok, Type *type);
Node *vardec_and_funcdef_stmt(int is_static, int is_extern);
Node *struct_stmt();
Node *typedef_stmt();
Node *handle_array_initialization(Node *node, Type *type, Type *org_type);
Node *handle_scalar_initialization(Node *node, Type *type);
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, Type *org_type, int set_offset);

// stmt.c
Node *block_stmt();
Node *goto_stmt();
Node *label_stmt();
Node *if_stmt();
Node *while_stmt();
Node *do_while_stmt();
Node *for_stmt();
Node *break_stmt();
Node *continue_stmt();
Node *return_stmt();
Node *stmt();

// parse.c
void error_duplicate_name(Token *tok, const char *type);
void *safe_realloc_array(void *ptr, int element_size, int new_size);
void program();

// expr.c
Node *expr();
Node *assign_sub(Node *lhs, Node *rhs, char *ptr);
Node *assign();
Node *logical_or();
Node *logical_and();
Node *bit_or();
Node *bit_xor();
Node *bit_and();
Node *equality();
Node *relational();
Node *bit_shift();
Node *new_add(Node *lhs, Node *rhs, char *ptr);
Node *new_sub(Node *lhs, Node *rhs, char *ptr);
Node *add();
Type *resolve_type_mul(Type *left, Type *right, char *ptr);
Node *mul();
Node *unary();
Node *increment_decrement();
Node *access_member();
Node *primary();

//
// Code generation
//

char *regs1(int i);
char *regs4(int i);
char *regs8(int i);
void gen_rodata_section();
void gen_data_section();
void gen_text_section();
void gen_lval(Node *node);
void asm_memcpy(Node *lhs, Node *rhs);
void generate_assembly();
void gen(Node *node);

// globals.c
void init_global_variables();

// file.c
typedef enum { SEEK_SET, SEEK_CUR, SEEK_END } SeekWhence;
char *read_file();
char *find_file_includes(char *name);

//
// Extensions
//

// extension.c
extern void write_file(char *fmt, ...);
extern void error(char *fmt, ...);
extern void error_at(char *loc, char *fmt, ...);
extern void warning_at(char *loc, char *fmt, ...);

//
// Standard library functions
//

// stdio.h
extern int printf(char *fmt, ...);
typedef struct _IO_FILE FILE;
extern FILE *fopen(const char *filename, const char *mode);
extern int fprintf();
extern int fclose();
extern int fseek();
extern int ftell();
extern int fread();

// ctype.h
extern int isspace();
extern int isdigit();

// string.h
extern int memcmp();
extern int memcpy();
extern int strlen();
extern int strtol();
extern char *strstr();
extern char *strchr();

// stdlib.h
extern void *malloc();
extern void *realloc();
extern void free();
