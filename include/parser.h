#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

#define MAX_FUNC_PARAMS 16

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
  String **str;
  int val_cap;
  int str_cap;
  int byte;
  int len;        // 配列の要素数
  int init;       // 初期化されている数
  int elem_count; // 最上位次元で指定された要素数
  int id;
};

typedef struct StructLiteral StructLiteral;
struct StructLiteral {
  StructLiteral *next;
  unsigned char *data;
  int size;
  int id;
  int needs_label;
};

typedef enum {
  TY_NONE,
  TY_BOOL,
  TY_INT,
  TY_CHAR,
  TY_SHORT,
  TY_LONG,
  TY_LONGLONG,
  TY_PTR,
  TY_ARR,
  TY_ARGARR,
  TY_VOID,
  TY_STRUCT,
  TY_UNION,
  TY_FUNC
} TypeKind;

typedef struct Type Type;
struct Type {
  TypeKind ty;
  Type *ptr_to;
  int array_size;
  struct Object *object;
  int is_const;
  int is_unsigned;
  Type *return_type;
  Type *param_types[MAX_FUNC_PARAMS];
  Token *param_names[MAX_FUNC_PARAMS];
  int param_count;
  int is_variadic;
};

typedef struct TypeTag TypeTag;
struct TypeTag {
  TypeTag *next;
  Type *type;
  char *name;
  int len;
};

typedef struct LVar LVar;
struct LVar {
  LVar *next;
  char *name;
  int len;
  int offset;
  int is_extern;
  Type *type;
  int is_static;
  int block;
  int is_bitfield;
  int bit_width;
  int bit_offset;
  Array *init_array;
  StructLiteral *init_struct;
};

typedef struct LVarList LVarList;
struct LVarList {
  LVarList *next;
  LVar *var;
};

typedef struct Object Object;
struct Object {
  Object *next;
  LVar *var;
  char *name;
  int len;
  int size;
  int is_defined;
};

typedef struct ObjectList ObjectList;
struct ObjectList {
  ObjectList *next;
  Object *object;
};

typedef struct Label Label;
struct Label {
  Label *next;
  char *name;
  int len;
  int id;
};

typedef enum {
  BUILTIN_FN_NONE = 0,
  BUILTIN_FN_MEMCPY_CHK,
  BUILTIN_FN_MEMMOVE_CHK,
  BUILTIN_FN_MEMSET_CHK,
  BUILTIN_FN_STRCPY_CHK,
  BUILTIN_FN_STRNCPY_CHK,
  BUILTIN_FN_STPCPY_CHK,
  BUILTIN_FN_SNPRINTF_CHK,
  BUILTIN_FN_VSNPRINTF_CHK,
  BUILTIN_FN_OBJECT_SIZE
} BuiltinFunctionKind;

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  int len;
  int offset;
  int is_static;
  Label *labels;
  Type *type;
  int type_check;
  int is_defined;
  BuiltinFunctionKind builtin_kind;
  Function *builtin_alias;
  int is_inline;
};

typedef enum {
  ND_NONE,           // 空のノード
  ND_ADD,            // +
  ND_SUB,            // -
  ND_MUL,            // *
  ND_DIV,            // /
  ND_MOD,            // %
  ND_EQ,             // ==
  ND_NE,             // !=
  ND_LT,             // <
  ND_LE,             // <=
  ND_AND,            // &&
  ND_OR,             // ||
  ND_NOT,            // !
  ND_BITNOT,         // ~
  ND_BITAND,         // &
  ND_BITOR,          // |
  ND_BITXOR,         // ^
  ND_SHL,            // <<
  ND_SHR,            // >>
  ND_ASSIGN,         // =
  ND_POSTINC,        // ++ or --
  ND_LVAR,           // ローカル変数
  ND_VARDEC,         // 変数宣言
  ND_GVAR,           // グローバル変数
  ND_GLBDEC,         // グローバル変数宣言
  ND_NUM,            // 整数
  ND_STRING,         // 文字列
  ND_ARRAY,          // 配列
  ND_ADDR,           // &
  ND_DEREF,          // *
  ND_SWITCH,         // switch
  ND_CASE,           // case
  ND_DEFAULT,        // default
  ND_IF,             // if
  ND_ELSE,           // else
  ND_WHILE,          // while
  ND_FOR,            // for
  ND_BREAK,          // break
  ND_CONTINUE,       // continue
  ND_DOWHILE,        // do-while
  ND_TERNARY,        // 三項演算子
  ND_GOTO,           // goto
  ND_LABEL,          // ラベル
  ND_RETURN,         // return
  ND_ASM,            // inline asm statement (ignored)
  ND_FUNCDEF,        // 関数定義
  ND_FUNCALL,        // 関数呼び出し
  ND_FUNCNAME,       // 関数名
  ND_EXTERN,         // extern
  ND_BLOCK,          // { ... }
  ND_ENUM,           // 列挙体
  ND_UNION,          // union
  ND_STRUCT,         // 構造体
  ND_STRUCT_LITERAL, // 構造体リテラル
  ND_TYPEDEF,        // typedef
  ND_TYPE,           // 型
  ND_TYPECAST,       // 型キャスト
  ND_COMMA,          // コンマ演算子
  ND_STMTEXPR        // GNU statement expression ({ ... })
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  int val;
  int id;
  int endline;
  int *cases;
  int case_cnt;
  int case_cap;
  int has_default;
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *step;
  Node **body;
  Node *args[MAX_FUNC_PARAMS];
  Function *fn;
  LVar *var;
  Type *type;
  Label *label;
};

void program();
Node *expr();

// Builtin lowering
void initialize_builtin_functions();

// Symbol helpers
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

// Type system helpers
Type *parse_base_type_internal(int should_consume, int should_record);
Type *peek_base_type();
Type *parse_pointer_qualifiers(Type *base_type);
Type *parse_declarator(Type *base_type, Token **tok, char *stmt);
Type *parse_declarator_suffix(Type *type, char *stmt);
Type *parse_function_suffix(Type *type, char *stmt);
Type *consume_type(int should_record);
Type *new_type(TypeKind ty);
Type *new_type_ptr(Type *ptr_to);
Type *new_type_arr(Type *ptr_to, int array_size);
Type *parse_array_dimensions(Type *base_type);
Type *max_type(Type *lhs, Type *rhs);
void substitute_type(Type *where, Type *placeholder, Type *actual);
int is_type(Token *tok);
int is_ptr_or_arr(Type *type);
int is_number(Type *type);
int is_enum_type(Type *type);
int get_sizeof(Type *type);
int type_size(Type *type);
char *type_name(Type *type);
int is_type_compatible(Type *lhs, Type *rhs);
int is_type_identical(Type *lhs, Type *rhs);
int is_type_assignable(Type *lhs, Type *rhs);
int eval_const_expr(Node *node, int *ok);

#endif // PARSER_H
