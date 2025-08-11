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
  TK_GOTO,     // GOTO
  TK_STRING,   // 文字列
  TK_TYPEDEF,  // typedef
  TK_ENUM,     // enum
  TK_UNION,    // union
  TK_STRUCT    // struct
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
  int len;  // 配列の要素数
  int init; // 初期化されている数
  int id;
};

typedef enum { TY_NONE, TY_INT, TY_CHAR, TY_PTR, TY_ARR, TY_ARGARR, TY_VOID, TY_STRUCT, TY_UNION } TypeKind;

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

// struct, enum, unionの型
typedef struct Object Object;
struct Object {
  Object *next;   // 次の Object か NULL
  LVar *var;      // 次の変数かNULL
  char *name;     // 変数の名前
  int len;        // 名前の長さ
  int size;       // 構造体のサイズ
  int is_defined; // 定義済みかどうか
};

typedef struct TypeTag TypeTag;
struct TypeTag {
  TypeTag *next;  // 次の構造体かNULL
  Object *object; // struct または union の型
  TypeKind kind;  // タグの型
  char *name;     // タグの名前
  int len;        // 名前の長さ
};

struct Type {
  TypeKind ty;
  Type *ptr_to;
  int array_size;
  Object *object;
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
  Function *next;       // 次の関数かNULL
  char *name;           // 変数の名前
  int len;              // 名前の長さ
  int offset;           // RBPからのオフセット
  Type *type;           // 関数の型
  int is_static;        // staticかどうか
  Label *labels;        // ラベルのリスト
  Type *return_type;    // 戻り値の型
  Type *param_types[6]; // 引数の型の配列
  int param_count;      // 引数の数
  int type_check;       // 型チェックするかどうか
  int is_defined;       // 定義済みかどうか
};

// lexer.c からエクスポートする関数
int startswith(char *p, char *q);
int is_alnum(char c);
void new_token(TokenKind kind, char *str, int len);
char *handle_include_directive(char *p);

// token_parser.c からエクスポートする関数
char *parse_number_literal(char *p);
char *parse_string_literal(char *p);
char *parse_char_literal(char *p);
void tokenize();

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
  ND_SWITCH,   // switch
  ND_CASE,     // case
  ND_DEFAULT,  // default
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
  ND_UNION,    // union
  ND_STRUCT,   // 構造体
  ND_TYPEDEF,  // typedef
  ND_TYPE,     // 型
  ND_TYPECAST  // 型キャスト
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
  int *cases;      // kindがND_SWITCHの場合のみ使う
  int case_cnt;    // kindがND_SWITCHの場合のみ使う
  int has_default; // kindがND_SWITCHの場合のみ使う
  Node *cond;      // kindがND_IF, ND_WHILE, ND_FORの場合のみ使う
  Node *then;      // kindがND_IFの場合のみ使う
  Node *els;       // kindがND_IFの場合のみ使う
  Node *init;      // kindがND_FORの場合のみ使う
  Node *step;      // kindがND_FORの場合のみ使う
  Node **body;     // kindがND_BLOCKの場合のみ使う
  Node *args[6];   // kindがND_FUNCALLの場合のみ使う
  Function *fn;    // kindがND_FUNCDEF, ND_FUNCALLの場合のみ使う
  LVar *var;       // kindがND_LVAR, ND_GVARの場合のみ使う
  Type *type;
  Label *label;
};

// symbol.c
Node *new_node(NodeKind kind);
Node *new_num(int val);
Node *new_deref(Node *lhs);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
LVar *new_lvar(Token *tok, Type *type, int is_static, int is_extern);
LVar *find_lvar(Token *tok);
LVar *find_gvar(Token *tok);
Object *find_struct(Token *tok);
Object *find_union(Token *tok);
Object *find_enum(Token *tok);
LVar *find_enum_member(Token *tok);
LVar *find_object_member(Object *object, Token *tok);
TypeTag *find_type_tag(Token *tok);
Function *find_fn(Token *tok);

// parse.c
void error_duplicate_name(Token *tok, const char *type);
void *safe_realloc_array(void *ptr, int element_size, int new_size);
void program();
int peek(char *op);
int consume(char *op);
Token *consume_ident();
Token *expect_ident(char *stmt);
void expect(char *op, char *err, char *st);
int expect_number(char *stmt);
int parse_sign();
int expect_signed_number();
String *string_literal();
Array *array_literal();

// type.c
Type *parse_base_type_internal(int should_consume, int should_record);
Type *peek_base_type();
Type *parse_pointer_qualifiers(Type *base_type);
Type *consume_type(int should_record);
int is_type(Token *tok);
int is_ptr_or_arr(Type *type);
int is_number(Type *type);
int get_sizeof(Type *type);
int type_size(Type *type);
Type *new_type(TypeKind ty);
Type *new_type_ptr(Type *ptr_to);
Type *new_type_arr(Type *ptr_to, int array_size);
Type *parse_array_dimensions(Type *base_type);
char *type_name(Type *type);
int is_same_type(Type *lhs, Type *rhs);

// decl.c
Node *function_definition(Token *tok, Type *type, int is_static);
Node *local_variable_declaration(Token *tok, Type *type, int is_static);
Node *global_variable_declaration(Token *tok, Type *type, int is_static);
Node *extern_variable_declaration(Token *tok, Type *type);
Node *vardec_and_funcdef_stmt(int is_static, int is_extern);
Object *struct_and_union_declaration(const int is_struct, const int is_union, const int should_record,
                                     const int is_typedef);
Object *enum_declaration(const int should_record, const int is_typedef);
Node *typedef_stmt();
Node *handle_array_initialization(Node *node, Type *type, Type *org_type);
Node *handle_scalar_initialization(Node *node, Type *type, char *ptr);
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
Node *switch_stmt();
Node *case_stmt();
Node *default_stmt();
int check_label();
Node *expression_stmt();
Node *stmt();

// expr.c
Node *expr();
Node *assign_sub(Node *lhs, Node *rhs, char *ptr, int check_const);
Node *assign();
Node *type_cast();
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
int compile_time_number();

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
extern void warning(char *fmt, ...);
extern void warning_at(char *loc, char *fmt, ...);

//
// Standard library functions
//

// stdio.h
extern int printf(char *fmt, ...);
extern int sprintf(char *fmt, ...);
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
extern int strcmp();
extern int strncmp();
extern int memcpy();
extern int strlen();
extern int strtol();
extern char *strstr();
extern char *strchr();

// stdlib.h
extern void *malloc();
extern void *realloc();
extern void free();
